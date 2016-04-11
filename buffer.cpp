//
// Created by jackson on 11.04.16.
//

#include <assert.h>
#include "buffer.h"
#include "util.h"

const char * buffer::get(int amount) {
    assert(amount <= data.length());

    const char * res = data.c_str() + offset;

    offset += amount;

    if (data.length() >= 10000 || offset == data.length()) {
        trim();
    }

    return res;
}

void buffer::put(const char *const data, size_t length) {
    this->data += std::string(data, length);
}

bool buffer::empty() {
    return data.empty();
}

int buffer::length() {
    return (int) (data.length() - offset);
}

const std::string& buffer::string_data() {
    trim();
    return data;
}

void buffer::clear() {
    std::string().swap(data);
    offset = 0;
}

void buffer::trim() {
    data = data.substr(offset, data.length() - offset);
    offset = 0;
}