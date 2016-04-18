//
// Created by jackson on 11.04.16.
//

#ifndef KIMBERLY_BUFFER_H
#define KIMBERLY_BUFFER_H

#include <string>
#include <string.h>

class buffer {
public:
    // !caution! don't call trim() (and clear() and put() everything changing data) immediately after this method
    const char *get(int amount);

    // stashing put or get. otherwise throwing logic_error
    void stash();

    void set(const char *const data);

    void put(const char *const data, size_t length);

    void clear();

    bool empty();

    //minus "offset"
    int length();

    const std::string &string_data();

private:
    std::string data;
    size_t offset = 0;

    enum {PUT, SET, GET, CLEAR, TRIM, STASH, NOT_MODIFIED} last_method = NOT_MODIFIED;
    union{
        size_t prev_offset;
        size_t prev_data_size;
    };

    // trim an offset
    void trim();
};

#endif //KIMBERLY_BUFFER_H