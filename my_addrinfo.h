//
// Created by jackson on 29.04.16.
//

#ifndef KIMBERLY_MY_ADDRINFO_H
#define KIMBERLY_MY_ADDRINFO_H

#include <netdb.h>

class my_addrinfo {
    struct addrinfo *info;

public:
    my_addrinfo() {
        info = nullptr;
    }

    my_addrinfo(my_addrinfo &&rhs) {
        info = rhs.info;
        rhs.info = nullptr;
    }

    my_addrinfo(struct addrinfo *info) : info(info) { }

    ~my_addrinfo() {
        if (info != nullptr)
            freeaddrinfo(info);
    }

    struct addrinfo* get_info() {
        return info;
    }
};

#endif //KIMBERLY_MY_ADDRINFO_H
