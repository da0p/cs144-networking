#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

#include <iostream>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::send_segment(TCPSegment segment) {
    /* Wrap seqno */
    segment.header().seqno = next_seqno();
    /* add bytes in flight */
    _bytes_in_flight += segment.length_in_sequence_space();
    /* calculate next_seqno */
    _next_seqno += segment.length_in_sequence_space();
    /* push in segment out */
    _segments_out.push(segment);
    /* insert in ordered map to search later */
    _outstanding_segments.insert(std::pair<uint64_t, TCPSegment>(_next_seqno, segment));
    /* start timer if not */
    if (!_timer_on) {
        _elapsed_time = 0;
        _rto = _initial_retransmission_timeout;
        _timer_on = true;
    }
}

void TCPSender::fill_window() {

    TCPSegment segment;
    string payload;

    /* For SYN */
    if (!_syn) {
        _syn = true;
        segment.header().syn = true;
        send_segment(segment);
        return;
    }

    while (stream_in().buffer_size() > 0 && _recv_wdn_sz > 0) { 
        /* Read from buffer */
        if (_recv_wdn_sz > TCPConfig::MAX_PAYLOAD_SIZE) {
            segment.payload() = stream_in().read(TCPConfig::MAX_PAYLOAD_SIZE);
            _recv_wdn_sz -= segment.length_in_sequence_space();
        }
        else {
            segment.payload() = stream_in().read(_recv_wdn_sz);
             _recv_wdn_sz -= segment.length_in_sequence_space();
        }

        if (stream_in().eof() && _recv_wdn_sz > 0) {
            /* Put FIN flag, piggy-backed FIN*/
            segment.header().fin = true;
            _fin_sent = true;
            _recv_wdn_sz -= 1;
        }
       /* Only send when segment occupies sequence space */
        if (segment.length_in_sequence_space() > 0) {
            send_segment(segment);
        }
    }

    //! If can't piggy-back FIN, do it next time
    if (!_fin_sent && stream_in().eof() && _recv_wdn_sz > 0) {
        segment.header().fin = true;
        _recv_wdn_sz -= 1;
        send_segment(segment);
        _fin_sent = true;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 

    /* window size handling */
    uint64_t abs_ackno = unwrap(ackno, _isn, _abs_ackno);
    TCPSegment seg;

    if (abs_ackno > next_seqno_absolute())
        return;

    if (abs_ackno + window_size < next_seqno_absolute()) {
        _recv_wdn_sz = 0;
        return;
    }
    
    _abs_ackno = abs_ackno;
    _wdn_sz = window_size;

    std::map<uint64_t, TCPSegment>::iterator it;
    it = _outstanding_segments.find(_abs_ackno);
    if (it != _outstanding_segments.end()) {
        for (auto _it = _outstanding_segments.begin(); _it != it; ++_it) {
            _bytes_in_flight -= (_it->second).length_in_sequence_space();
        }
        _outstanding_segments.erase(_outstanding_segments.begin(), it);
        _bytes_in_flight -= (it->second).length_in_sequence_space();
        _outstanding_segments.erase(it);
    }

    _recv_wdn_sz = abs_ackno + window_size - next_seqno_absolute(); 

    _recv_wdn_sz = _wdn_sz == 0 ? 1 : _recv_wdn_sz;

    /* Only reset timer when there is a new bigger ackno */
    if (_abs_ackno > _max_abs_seqno) {
        _max_abs_seqno = _abs_ackno;
        _rto = _initial_retransmission_timeout;
        _consecutive_retransmissions = 0;
        //! restart the timer when there is still outstanding segments
        if (!_outstanding_segments.empty()) {
            _elapsed_time = 0;
        }
    }

    //! stop the timer when there is no outstanding segments
    if (_outstanding_segments.empty())
        _timer_on = false;

    //! Avoid fill the window when the connection is not established
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    TCPSegment seg;
    /* Only check timeout if timer is on */
    if (_timer_on) {
        if (!_outstanding_segments.empty())
            _elapsed_time += ms_since_last_tick;
        else {
            _rto = _initial_retransmission_timeout;
            _elapsed_time = 0;
        }

        if (_elapsed_time >= _rto) {
            if (!_outstanding_segments.empty()) {
                auto it = _outstanding_segments.begin();
                seg = it->second;
                _segments_out.push(seg);
            }
            /* Back-off timer when window size is 0, or the receiver's window is
             * full */
            if (_wdn_sz > 0) {
                _consecutive_retransmissions++;
                _rto *= 2;
            }
            /* reset elapsed time */
            _elapsed_time = 0;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() 
{
    TCPSegment segment;

    segment.header().ack = true;

    segment.header().seqno = next_seqno();

    segments_out().push(segment);
}
