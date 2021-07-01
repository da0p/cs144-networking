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
    /* Wrap seqno */
    segment.header().seqno = next_seqno();
    /* add bytes in flight */
    tcps_bytes_in_flight += segment.length_in_sequence_space();
    /* calculate next_seqno */
    _next_seqno += segment.length_in_sequence_space();
    /* push in segment out */
    _segments_out.push(segment);
    /* insert in ordered map to search later */
    tcps_outstanding_segments.insert(std::pair<uint64_t, TCPSegment>(_next_seqno, segment));
    /* start timer if not */
    if (!tcps_timer_on) {
        tcps_elapsed_time = 0;
        tcps_rto = _initial_retransmission_timeout;
        tcps_timer_on = true;
    }
}

void TCPSender::fill_window() {

    TCPSegment segment;
    string payload;

    /* For SYN */
    if (!tcps_syn) {
        tcps_syn = true;
        segment.header().syn = true;
        send_segment(segment);
        return;
    }

    /* For SYN-ACK */
    if (tcps_syn && !tcps_syn_ack) {
       tcps_syn_ack = true;
       return;
    }

    if (tcps_syn  && !tcps_fin && (stream_in().buffer_size() > 0 || stream_in().eof())) {
        if (tcps_window_size > 0) { 
            do {
                /* Read from buffer */
                if (tcps_window_size > TCPConfig::MAX_PAYLOAD_SIZE) {
                    segment.payload() = stream_in().read(TCPConfig::MAX_PAYLOAD_SIZE);
                }
                else {
                    segment.payload() = stream_in().read(tcps_window_size);
                }
               
                /* Put FIN flag */
                if (stream_in().eof() && segment.length_in_sequence_space() + tcps_bytes_in_flight < tcps_window_size) {
                    segment.header().fin = true;
                    tcps_fin = true;
                }
               
                /* Only send when segment occupies sequence space */
                if (segment.length_in_sequence_space() > 0) {
                    tcps_window_size -= segment.length_in_sequence_space();
                    send_segment(segment);
                }

            } while (stream_in().buffer_size() > 0 && tcps_window_size > 0); 
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 

    std::map<uint64_t, TCPSegment>::iterator it;
    std::map<uint64_t, TCPSegment>::iterator iit;
   
    /* window size handling */
    tcps_window_size = window_size;
    if (tcps_window_size == 0) {
        tcps_window_size = 1;
        tcps_receiver_full = true;
    } else 
        tcps_receiver_full = false;

    tcps_next_segno_ack = unwrap(ackno, _isn, tcps_next_segno_ack);
    //! look for the ackno key in the map
    it = tcps_outstanding_segments.find(tcps_next_segno_ack);

    //! If found, then delete all elements before 
    if (it != tcps_outstanding_segments.end()) {
        for (iit = tcps_outstanding_segments.begin(); iit != it; ++iit)
            tcps_bytes_in_flight -= (iit->second).length_in_sequence_space();

        tcps_outstanding_segments.erase(tcps_outstanding_segments.begin(), it);
        tcps_outstanding_segments.erase(it);
        tcps_bytes_in_flight -= (it->second).length_in_sequence_space();
    }
    
    /* Only reset timer when there is a new bigger ackno */
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
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    std::map<uint64_t, TCPSegment>::iterator iit;

    /* Only check timeout if timer is on */
    if (tcps_timer_on) {
        if (!tcps_outstanding_segments.empty())
            tcps_elapsed_time += ms_since_last_tick;
        else {
            tcps_rto = _initial_retransmission_timeout;
            tcps_elapsed_time = 0;
        }

        if (tcps_elapsed_time >= tcps_rto) {
            iit = tcps_outstanding_segments.begin();
            _segments_out.push(iit->second);
            /* Back-off timer when window size is 0, or the receiver's window is
             * full */
            if (!tcps_receiver_full) {
                tcps_consecutive_retransmissions++;
                tcps_rto *= 2;
            }
            /* reset elapsed time */
            tcps_elapsed_time = 0;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return tcps_consecutive_retransmissions; }

void TCPSender::send_empty_segment() 
{
    TCPSegment empty_tcp_segment;

    send_segment(empty_tcp_segment);
}
