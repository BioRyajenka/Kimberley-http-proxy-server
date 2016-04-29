//
// Created by jackson on 29.04.16.
//

#ifndef KIMBERLY_FILE_DESCRIPTOR_H
#define KIMBERLY_FILE_DESCRIPTOR_H

#include <unistd.h>

class file_descriptor {
public:
    file_descriptor(int fd) : fd(fd) {}
    file_descriptor() : fd(0) {}
    ~file_descriptor() {close(fd);}

    int get_fd() const {
        return fd;
    }
private:
    int fd;
};

#endif //KIMBERLY_FILE_DESCRIPTOR_H
