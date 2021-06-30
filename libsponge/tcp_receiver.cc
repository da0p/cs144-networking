#include <iostream>
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

    if (seg.header().syn) {
        tcpr_isn = seg.header().seqno;
        tcpr_syn = true;
    }
    
    if (tcpr_syn) {
        tcpr_checkpoint = unwrap(seg.header().seqno, tcpr_isn, tcpr_checkpoint);
        payload = seg.payload().copy();
        if (seg.header().fin)
            tcpr_end = true;

        _reassembler.push_substring(payload, seg.header().syn ? 0 : tcpr_checkpoint - 1, tcpr_end);

        bf_bytes_written = _reassembler.stream_out().bytes_written();

        /* For SYN */
        tcpr_exp_ack = bf_bytes_written + 1; 
        
        /* for FIN */
        if (tcpr_end && _reassembler.empty())
            tcpr_exp_ack += 1;

        tcpr_ack = wrap(tcpr_exp_ack, tcpr_isn);
    }
}


optional<WrappingInt32> TCPReceiver::ackno() const { 

    if (tcpr_syn) {

        return tcpr_ack;
    }
    return {}; 
}

size_t TCPReceiver::window_size() const { 
    return _capacity - _reassembler.stream_out().buffer_size();  
}
