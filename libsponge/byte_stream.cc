#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) {
    bs_cap = capacity;
    bs_total_written_bytes = 0;
    bs_total_read_bytes = 0;
    bs_input_ended = false;
}

size_t ByteStream::write(const string &data) {
    size_t len = data.length();
    size_t n_written_bytes = 0;
    size_t rem_cap = remaining_capacity();

    if (rem_cap > 0) {
        if (rem_cap < len) {
            n_written_bytes = rem_cap;
            Buffer bf{data.substr(0, n_written_bytes)};
            bs_buf.append(bf);
            bs_total_written_bytes += n_written_bytes;
        }
        else {
            n_written_bytes = len;
            Buffer bf{string(data)};
            bs_buf.append(bf);
            bs_total_written_bytes += n_written_bytes;
        }
    }
    return n_written_bytes;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string buf = bs_buf.concatenate();
    string fr = buf;
    size_t blen = buf.length();

    if (blen > len) {
        fr = buf.substr(0, len);
    }
    
    return fr;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t size = buffer_size();

    if (size > len) {
        bs_buf.remove_prefix(len);

        bs_total_read_bytes += len;
    }
    else {
        bs_buf.remove_prefix(size);

        bs_total_read_bytes += size;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string read_bytes;

    read_bytes = peek_output(len);
    pop_output(len);

    return read_bytes;
}

void ByteStream::end_input() {bs_input_ended = true;}

bool ByteStream::input_ended() const { return bs_input_ended; }

size_t ByteStream::buffer_size() const { return bs_buf.size(); }

bool ByteStream::buffer_empty() const { return bs_buf.size() == 0 ? true : false; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return bs_total_written_bytes; }

size_t ByteStream::bytes_read() const { return bs_total_read_bytes; }

size_t ByteStream::remaining_capacity() const {
    size_t rem_cap = 0;
    if (bs_cap < bs_buf.size())
        rem_cap = 0;
    else
        rem_cap = bs_cap - bs_buf.size();

    return rem_cap;
}

