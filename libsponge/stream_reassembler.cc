#include <iostream>
#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : 
    _output(capacity),
    _capacity(capacity), 
    sr_unassembled_bytes(0), 
    sr_expected_index(0),
    sr_rem_cap(capacity),
    sr_buf(capacity, ""),
    sr_buf_state(capacity, false),
    input_ended(false)
{
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    
    size_t sr_upper_bound_index = sr_expected_index + _capacity;
    size_t bs_rem_cap;
    size_t len = data.length();
    size_t i;
    size_t k;
    string mod_data = data;
    string tmp;
    size_t mod_index = index;


    if (len > 0) {
        if (index <= sr_expected_index && index + len >= sr_upper_bound_index) {
            mod_data = data.substr(sr_expected_index - index, sr_rem_cap);
            mod_index = sr_expected_index;
        }
        else if (index <= sr_expected_index && index + len < sr_upper_bound_index) {
            if (index + len <= sr_expected_index) return;
            else {
                mod_data = data.substr(sr_expected_index - index, len + index - sr_expected_index);
                mod_index = sr_expected_index;
            }
        }
        else if (index > sr_expected_index && index < sr_upper_bound_index) {
            if (index + len > sr_upper_bound_index) {
                mod_data = data.substr(0, sr_upper_bound_index - index); 
                mod_index = index;
            }
        }
        else if (index >= sr_upper_bound_index) return;
    }
    len = mod_data.length();
    for (i = mod_index; i < mod_index + len; i++) {
        if (!sr_buf_state[i % _capacity]) {
            sr_buf[i % _capacity] = mod_data.substr(i - mod_index, 1); 
            sr_buf_state[i % _capacity] = true;
            sr_unassembled_bytes++;
            sr_rem_cap--;
        }
    }
    
    i = 0;
    k = sr_expected_index % _capacity;
    bs_rem_cap = _output.remaining_capacity();
    while (sr_buf_state[k] && bs_rem_cap > 0) {
        tmp += sr_buf[k];
        sr_buf_state[k] = false;
        sr_unassembled_bytes--;
        sr_expected_index++;
        sr_rem_cap++;
        bs_rem_cap--;
        k = sr_expected_index % _capacity;
        i++;
    }

    if (tmp.length() > 0)
        _output.write(tmp);

    if (eof)
        input_ended = true;

    if (input_ended && empty()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return sr_unassembled_bytes; }

bool StreamReassembler::empty() const { return sr_unassembled_bytes == 0; }
