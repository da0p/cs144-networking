#include "tcp_connection.hh"

#include <iostream>
#include <limits>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { 
    return _sender.stream_in().remaining_capacity(); 
}

size_t TCPConnection::bytes_in_flight() const { 
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const { 
    return _receiver.unassembled_bytes(); 
}

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {

    if (!active())
        return;
    
    //! reset when received a segment
    _time_since_last_segment_received = 0;

    if (seg.header().rst) {
        //! Set inbound and outbound stream to error
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _linger_after_streams_finish = false;
        //! Kill the connection permanently
        return;
    }
    else {
        _receiver.segment_received(seg);
        if (seg.header().ack) {
            _sender.ack_received(seg.header().ackno, seg.header().win);
        }
        //! If the message occupies any sequence space, reply
        if (seg.length_in_sequence_space() > 0) {
            if (seg.header().syn && !seg.header().ack && !_initiator) {
                //! to get SYN
                _sender.fill_window();
            }
            else
                _sender.send_empty_segment();
            if (!_sender.segments_out().empty()) {
                TCPSegment ack_seg = _sender.segments_out().front();
                _sender.segments_out().pop();
                if (_receiver.ackno().has_value())
                    ack_seg.header().ackno = _receiver.ackno().value();
                if (!ack_seg.header().ack)
                    ack_seg.header().ack = true;
                if (_receiver.window_size() > std::numeric_limits<uint16_t>::max())
                    ack_seg.header().win = std::numeric_limits<uint16_t>::max();
                else
                    ack_seg.header().win = _receiver.window_size();

                segments_out().push(ack_seg);
            }
        }
        else {
            //! sequence space == 0 --> only ACK
            flush_buffer();
        }
    }
    //! The remote one is the one to end the stream first (passive
    //close)
    if (_receiver.stream_out().eof() && !_sender.stream_in().eof())
        _linger_after_streams_finish = false;


}

bool TCPConnection::active() const { 

    //! unclean shutdown
    if (_sender.stream_in().error() && _receiver.stream_out().error())
        return false;
   
 
    if (!_linger_after_streams_finish) {

        if (_receiver.stream_out().eof()  && _sender.stream_in().eof() && bytes_in_flight() == 0)
            return false;
    }
    else {
        if (_receiver.stream_out().eof() && 
            _sender.stream_in().eof() && 
            time_since_last_segment_received() >=10 * _cfg.rt_timeout)
            return false;
    }

    return true;
}

size_t TCPConnection::write(const string &data) {
    
    size_t written_bytes = 0;

    if (!active())
        return written_bytes;

    written_bytes =  _sender.stream_in().write(data);

    _sender.fill_window();

    flush_buffer();
    
    return written_bytes;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {

    TCPSegment seg;

    if (!active())
        return;

    _time_since_last_segment_received += ms_since_last_tick;

    _sender.tick(ms_since_last_tick);

    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        //! Unclean shutdown
        unclean_shutdown();
    }
    
    flush_buffer();
}

void TCPConnection::end_input_stream() {
    if (!active())
        return;
    _sender.stream_in().end_input();
    _sender.fill_window();
    flush_buffer(); 
}

void TCPConnection::connect() {
    //! Sending a SYN 
    TCPSegment seg;
    _sender.fill_window();
    if (!_sender.segments_out().empty()) {
        seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        segments_out().push(seg);
        _initiator = true;
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            flush_buffer();
            unclean_shutdown(); 
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::unclean_shutdown() {
    TCPSegment seg;

    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    if (_sender.segments_out().empty())
        _sender.send_empty_segment();
    seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    if (_receiver.ackno().has_value())
        seg.header().ackno = _receiver.ackno().value();
    seg.header().rst = true;
    segments_out().push(seg);
}

void TCPConnection::flush_buffer() {
    TCPSegment seg;

    while (!_sender.segments_out().empty()) {
        seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ackno = _receiver.ackno().value();
            if (_receiver.window_size() > std::numeric_limits<uint16_t>::max())
                seg.header().win = std::numeric_limits<uint16_t>::max();
            else
                seg.header().win = _receiver.window_size();
            seg.header().ack = true;
        }
        segments_out().push(seg);
    }

}
