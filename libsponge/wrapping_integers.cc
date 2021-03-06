#include <iostream>
#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {

    uint64_t isn_64t = static_cast<uint64_t> (isn.raw_value());
    uint32_t sn_32t; 
    
    sn_32t = (isn_64t + n) % (1UL << 32);
    
    return WrappingInt32(sn_32t);
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint32_t re_sq = static_cast<int64_t>(n.raw_value()) - static_cast<int64_t>(isn.raw_value());
    uint64_t abs_sq; 
    uint64_t MAX32_T = 1UL << 32;
    uint64_t HALF_MAX32_T = 1UL << 31;
    uint64_t k = checkpoint / MAX32_T;

    if (checkpoint <= static_cast<uint64_t>(re_sq)) {
        abs_sq = re_sq;
    }
    else {
        if (re_sq + MAX32_T * k > checkpoint + HALF_MAX32_T)
            abs_sq = re_sq + MAX32_T * (k - 1);
        else if (re_sq + MAX32_T * k + HALF_MAX32_T < checkpoint)
            abs_sq = re_sq + MAX32_T * (k + 1);
        else
            abs_sq = re_sq + MAX32_T * k;
    }

    return abs_sq;
}
