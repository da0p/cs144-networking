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
    _unassembled_bytes(0), 
    _expected_index(0),
    _rem_cap(capacity),
    _buf(capacity, ""),
    _buf_state(capacity, false),
    input_ended(false)
{
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    
    size_t _upper_bound_index = _expected_index + _capacity;
    size_t bs_rem_cap;
    size_t len = data.length();
    size_t i;
    size_t k;
    string mod_data = data;
    string tmp;
    size_t mod_index = index;


    if (len > 0) {
        if (index <= _expected_index && index + len >= _upper_bound_index) {
            mod_data = data.substr(_expected_index - index, _rem_cap);
            mod_index = _expected_index;
        }
        else if (index <= _expected_index && index + len < _upper_bound_index) {
            if (index + len <= _expected_index) return;
            else {
                mod_data = data.substr(_expected_index - index, len + index - _expected_index);
                mod_index = _expected_index;
            }
        }
        else if (index > _expected_index && index < _upper_bound_index) {
            if (index + len > _upper_bound_index) {
                mod_data = data.substr(0, _upper_bound_index - index); 
                mod_index = index;
            }
        }
        else if (index >= _upper_bound_index) return;
    }
    len = mod_data.length();
    for (i = mod_index; i < mod_index + len; i++) {
        if (!_buf_state[i % _capacity]) {
            _buf[i % _capacity] = mod_data.substr(i - mod_index, 1); 
            _buf_state[i % _capacity] = true;
            _unassembled_bytes++;
            _rem_cap--;
        }
    }
    
    i = 0;
    k = _expected_index % _capacity;
    bs_rem_cap = _output.remaining_capacity();
    while (_buf_state[k] && bs_rem_cap > 0) {
        tmp += _buf[k];
        _buf_state[k] = false;
        _unassembled_bytes--;
        _expected_index++;
        _rem_cap++;
        bs_rem_cap--;
        k = _expected_index % _capacity;
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

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
