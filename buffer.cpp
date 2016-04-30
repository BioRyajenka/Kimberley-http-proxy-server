//
// Created by jackson on 11.04.16.
//

#include <assert.h>
#include "buffer.h"
#include "util.h"

//!caution! after (trim) result of get will be unpredictable
const char *buffer::get(int amount) {
    assert(amount + offset <= data.length());

    if (data.length() >= 10000) {
        trim();
    }

    const char *res = data.c_str() + offset;

    prev_offset = offset;
    offset += amount;

    last_method = GET;

    return res;
}

void buffer::stash() {
    if (last_method == GET) {
        offset = prev_offset;
    } else if (last_method == PUT) {
        data = data.substr(0, prev_data_size);
    } else {
        throw new std::logic_error("Stashing not after put or get.");
    }
    last_method = STASH;
}

void buffer::put(const char *const data, size_t length) {
    prev_data_size = this->data.size();
    this->data += std::string(data, length);
    last_method = PUT;
}

void buffer::set(const char *const data) {
    clear();
    put(data, strlen(data));
    last_method = SET;
}

bool buffer::empty() {
    return length() == 0;
}

int buffer::length() {
    return (int) (data.length() - offset);
}

const std::string &buffer::string_data() {
    trim();
    return data;
}

void buffer::clear() {
    std::string().swap(data);
    offset = 0;
    last_method = CLEAR;
}

void buffer::trim() {
    data = data.substr(offset, data.length() - offset);
    offset = 0;
    last_method = TRIM;
}