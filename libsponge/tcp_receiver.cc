#include <iostream>
#include <limits>
#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    
    string payload;
    size_t bf_bytes_written;

    if (seg.header().syn && _syn)
        return;

    if (seg.header().syn) {
        _isn = seg.header().seqno;
        _syn = true;
    }
    
    if (_syn) {
        _checkpoint = unwrap(seg.header().seqno, _isn, _checkpoint);
        payload = seg.payload().copy();
        if (seg.header().fin)
            _end = true;

        _reassembler.push_substring(payload, seg.header().syn ? 0 : _checkpoint - 1, _end);

        bf_bytes_written = _reassembler.stream_out().bytes_written();

        /* For SYN */
        _exp_ack = bf_bytes_written + 1; 
        
        /* for FIN */
        if (_end && _reassembler.empty())
            _exp_ack += 1;

        _ack = wrap(_exp_ack, _isn);
    }
}


optional<WrappingInt32> TCPReceiver::ackno() const { 

    if (_syn) {

        return _ack;
    }
    return {}; 
}

size_t TCPReceiver::window_size() const { 
     return _capacity - _reassembler.stream_out().buffer_size();
}
