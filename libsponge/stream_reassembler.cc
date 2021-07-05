#include <iostream>
#include <tuple>
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
    input_ended(false)
{
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    
    std::string mdata;
    size_t mindex;
    bool verified = false;
    bool _eof = false;

    std::tie(mdata, mindex, _eof, verified) = validate(data, index, eof);

    if (!verified) return;

    if (!mdata.empty()) {
        merge(mdata,  mindex);

        reorder_buffer();
    }

    if (_eof)
        input_ended = true;

    if (input_ended && empty()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }

void StreamReassembler::merge(const std::string &data, size_t index) {
    std::map<size_t, std::string>::iterator it;
    std::map<size_t, std::string>::iterator it_prev;
    std::queue<std::map<size_t, std::string>::iterator> _to_be_deleted;
    size_t _index;
    std::string s;
    std::pair<size_t, std::string> p;
    bool _first = false;

    //! Only merge the first and the last element found, delete middle elements
    for (it = _buf.begin(); it != _buf.end(); ++it) {
        if (it->first + (it->second).length() >=index && it->first <= index + data.length()) {
            if (!_first) {
                if (index < it->first) {
                    _index = index;
                }
                else {
                    _index = it->first;
                    s = merge_string(it->second, it->first, data, index);
                }
                _first = true;
            }
            _to_be_deleted.push(it);
        }
        if (it->first > index + data.length()) {
           break; 
        }

        it_prev = it;
    }
    
    if (_first) {
        if (s.empty()) {
            s = merge_string(data, index, it_prev->second, it_prev->first); 
        }
        else {
            s = merge_string(s, _index, it_prev->second, it_prev->first); 
        }
    }

    if (!_to_be_deleted.empty()) {
        size_t _tot = 0;
        while (!_to_be_deleted.empty()) {
           auto iit =  _to_be_deleted.front();
           _tot += (iit->second).length();
            _buf.erase(iit);
            _to_be_deleted.pop();
        }
        p = make_pair(_index, s);
        _buf.insert(p);
        _unassembled_bytes -= _tot - s.length();
    }
    else {
        p = make_pair(index, data);
        _buf.insert(p);
        _unassembled_bytes += data.length();
    }
}

void StreamReassembler::reorder_buffer(void) {

    size_t bs_rem_cap;
    std::pair<size_t, std::string> p;

    if (!_buf.empty()) {
        auto it = _buf.begin();
        bs_rem_cap = _output.remaining_capacity();
        if (it->first == _expected_index && bs_rem_cap > 0) {
            if ((it->second).length() > bs_rem_cap) {
                auto tmp = (it->second).substr(0, bs_rem_cap);
                _output.write(tmp);
                _expected_index += tmp.length();
                _unassembled_bytes -= tmp.length();
                _buf.erase(it);
                //! create a new pair
                p = make_pair(_expected_index, (it->second).substr(bs_rem_cap, (it->second).length() - bs_rem_cap)); 
                _buf.insert(p);
            }
            else {
                //! just write and erase
                _output.write(it->second);
                _unassembled_bytes -= (it->second).length();
                _expected_index += (it->second).length();
                _buf.erase(it);
            }
        }
    }
}

std::tuple<std::string, size_t, bool, bool> StreamReassembler::validate(const std::string &data, size_t index, bool eof) {

    size_t _upper_bound_index = _expected_index + _output.remaining_capacity();
    size_t len = data.length();
    size_t mod_data_s = 0;
    string mod_data;
    size_t mod_index = index;
    bool _eof = eof;

    if (len > 0) {
        if (index <= _expected_index && index + len > _upper_bound_index) {
            mod_index = _expected_index;
            len = _upper_bound_index - _expected_index;
            mod_data_s = _expected_index - index;
            //! Discard eof if not fit in buffer
            _eof = false; 
        }
        else if (index <= _expected_index && index + len <= _upper_bound_index) {
            if (index + len <= _expected_index) return {std::string(), 0, false, _eof};
            else {
                mod_index = _expected_index;
                len = len + index - _expected_index;
                mod_data_s = _expected_index - index;
            }
        }
        else if (index > _expected_index && index < _upper_bound_index) {
            if (index + len > _upper_bound_index) {
                mod_index = index;
                len = _upper_bound_index - index;
                mod_data_s = 0;
                //! Discard eof if not fit in buffer
                _eof = false;
            }
        }
        else if (index >= _upper_bound_index) return {std::string(), 0, false, false};
    }
    mod_data = data.substr(mod_data_s, len);

    return {mod_data, mod_index, _eof, true};
}

std::string StreamReassembler:: merge_string(const std::string &str1, size_t ind1, const std::string &str2, size_t ind2) {
    
    size_t _overlapped;
    string s;

    if (ind1 < ind2 && ind1 + str1.length() < ind2) {
        return s;
    }

    if (ind2 < ind1 && ind2 + str2.length() < ind1) {
        return s;
    }

    if (ind1 <= ind2) {
        _overlapped = ind1 + str1.length() - ind2;
        if (_overlapped < str2.length())
            s = str1 + str2.substr(_overlapped, str2.length() - _overlapped);
        else
            s = str1;
    }
    else
    {
        _overlapped = ind2 + str2.length() - ind1;
        if (_overlapped < str1.length()) 
            s = str2 + str1.substr(_overlapped, str1.length() - _overlapped);
        else
            s = str2;
    }

    return s;
}
