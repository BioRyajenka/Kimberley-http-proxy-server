//
// Created by jackson on 11.04.16.
//

#include <assert.h>
#include "buffer.h"
#include "util.h"

//!caution! after (trim) result of get will be unpredictable
const char *buffer::get(int amount) {
    assert(amount + offset <= data.length());

    if (data.length() >= 10000 || offset == data.length()) {
        trim();
    }

    const char *res = data.c_str() + offset;

    offset += amount;

    return res;
}

void buffer::put(const char *const data, size_t length) {
    this->data += std::string(data, length);
}

bool buffer::empty() {
    return data.empty();
}

int buffer::length() {
    Log::d("data.length(): " + inttostr(data.length()));
    Log::d("offset: " + inttostr(offset));
    return (int) (data.length() - offset);
}

const std::string &buffer::string_data() {
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