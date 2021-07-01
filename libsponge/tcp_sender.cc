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

uint64_t TCPSender::bytes_in_flight() const { return tcps_bytes_in_flight; }

void TCPSender::send_segment(TCPSegment segment) {
    segment.header().seqno = next_seqno();
    tcps_bytes_in_flight += segment.length_in_sequence_space();
    _next_seqno += segment.length_in_sequence_space();
    _segments_out.push(segment);
    tcps_outstanding_segments.insert(std::pair<uint64_t, TCPSegment>(_next_seqno, segment));
    if (!tcps_timer_on) {
        tcps_elapsed_time = 0;
        tcps_rto = _initial_retransmission_timeout;
        tcps_timer_on = true;
    }
}

void TCPSender::fill_window() {

    TCPSegment segment;
    string payload;

    std::cout << "fill_window()" << std::endl;
    if (!tcps_syn) {
        tcps_syn = true;
        segment.header().syn = true;
        send_segment(segment);
        return;
    }

    if (tcps_syn && !tcps_syn_ack) {
       tcps_syn_ack = true;
       return;
    }

     std::cout << "tcps_fin = " << tcps_fin 
              << ", tcps_window_size = " << tcps_window_size 
              << ", bytes_in_flight = " << tcps_bytes_in_flight
              << ", buffer_size = " << stream_in().buffer_size()
              << ", eof = " << stream_in().eof() << std::endl;
    if ((stream_in().input_ended() || segment.header().fin) && tcps_window_size > tcps_bytes_in_flight + stream_in().buffer_size()  && !tcps_fin) {
        std::cout <<"Send FIN" << std::endl;
        segment.header().fin = true;
        segment.payload() = stream_in().read(tcps_window_size);
        send_segment(segment); 
        tcps_fin = true;
            std::cout << "fill_window() | _next_seqno = " << _next_seqno 
            << ", bytes_in_flight = " << tcps_bytes_in_flight << std::endl;

        return;
    }

    if (tcps_syn  && stream_in().buffer_size() > 0) {
        if (tcps_window_size > 0) { 
            do {
                if (tcps_window_size > TCPConfig::MAX_PAYLOAD_SIZE) {
                    segment.payload() = stream_in().read(TCPConfig::MAX_PAYLOAD_SIZE);
                }
                else {
                    segment.payload() = stream_in().read(tcps_window_size);
                }

                tcps_window_size -= segment.length_in_sequence_space();
                std::cout << "fill_window() | _next_seqno = " << _next_seqno 
                          << ", bytes_in_flight = " << tcps_bytes_in_flight << std::endl;
                send_segment(segment);
            } while (stream_in().buffer_size() > 0 && tcps_window_size > 0); 
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 

    std::map<uint64_t, TCPSegment>::iterator it;
    std::map<uint64_t, TCPSegment>::iterator iit;
   
    tcps_window_size = window_size;
    if (tcps_window_size == 0) {
        tcps_window_size = 1;
        tcps_receiver_full = true;
    } else 
        tcps_receiver_full = false;

    tcps_next_segno_ack = unwrap(ackno, _isn, tcps_next_segno_ack);

    std::cout << "ack_received() | tcps_next_segno_ack = " << tcps_next_segno_ack << std::endl;
    std::cout << "ack_received() | bytes_in_flight = " << tcps_bytes_in_flight << std::endl;

    //! look for the ackno key in the map
    it = tcps_outstanding_segments.find(tcps_next_segno_ack);

    //! If found, then delete all elements before 
    if (it != tcps_outstanding_segments.end()) {
        std::cout << " ack_received() | found something in collection" << std::endl;
        for (iit = tcps_outstanding_segments.begin(); iit != it; ++iit)
            tcps_bytes_in_flight -= (iit->second).length_in_sequence_space();

        tcps_outstanding_segments.erase(tcps_outstanding_segments.begin(), it);
        tcps_outstanding_segments.erase(it);
        tcps_bytes_in_flight -= (it->second).length_in_sequence_space();
    }
    
    if (tcps_next_segno_ack > tcps_segno_max_ack) {

        tcps_segno_max_ack = tcps_next_segno_ack;
        tcps_rto = _initial_retransmission_timeout;
        tcps_consecutive_retransmissions = 0;
        //! restart the timer when there is still outstanding segments
        if (!tcps_outstanding_segments.empty()) {
            tcps_elapsed_time = 0;
        }
    }

    //! stop the timer when there is no outstanding segments
    if (tcps_outstanding_segments.empty())
        tcps_timer_on = false;

    std::cout << "ack_received() | bytes_in_flight = " << tcps_bytes_in_flight << std::endl;

}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    std::map<uint64_t, TCPSegment>::iterator iit;

    if (tcps_timer_on) {
        if (!tcps_outstanding_segments.empty())
            tcps_elapsed_time += ms_since_last_tick;
        else {
            tcps_rto = _initial_retransmission_timeout;
            tcps_elapsed_time = 0;
        }

        std::cout << "tick() | elapsed_time = " << tcps_elapsed_time << std::endl;

        if (tcps_elapsed_time >= tcps_rto) {
            std::cout << "tick() | timeout" << std::endl;
            iit = tcps_outstanding_segments.begin();
            _segments_out.push(iit->second);
            std::cout << "tcps_window_size = " << tcps_window_size << std::endl;
            if (!tcps_receiver_full) {
                tcps_consecutive_retransmissions++;
                tcps_rto *= 2;
            }
            tcps_elapsed_time = 0;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return tcps_consecutive_retransmissions; }

void TCPSender::send_empty_segment() 
{
    TCPSegment empty_tcp_segment;
    WrappingInt32 wrapped_next_seqno = next_seqno();
    
    empty_tcp_segment.header().seqno = wrapped_next_seqno;

    _segments_out.push(empty_tcp_segment);
}
