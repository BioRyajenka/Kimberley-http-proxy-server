//
// Created by jackson on 11.04.16.
//

#ifndef KIMBERLY_BUFFER_H
#define KIMBERLY_BUFFER_H

#include <string>
#include <string.h>

class buffer {
public:
    // !caution! don't call trim() (length() etc) immediately after this method
    const char *get(int amount);

    void set(const char *const data) {
        clear();
        put(data, strlen(data));
    }

    void put(const char *const data, size_t length);

    void clear();

    bool empty();

    //minus "offset"
    int length();

    const std::string &string_data();

private:
    std::string data;
    size_t offset = 0;

    // trim an offset
    void trim();
};

#endif //KIMBERLY_BUFFER_H
