//
// Created by root on 08.01.16.
//

#ifndef KIMBERLY_HANDLER_H
#define KIMBERLY_HANDLER_H

#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "util.h"
#include "proxy_server.h"

class handler {
protected:
    int fd;
    proxy_server *serv;
public:
    virtual bool handle(epoll_event) = 0;
};

class echo_handler : public handler {
public:
    echo_handler(int sock, proxy_server *serv) {
        this->fd = sock;
        this->serv = serv;
    }

    bool handle(epoll_event);

private:
#define BUFFER_SIZE 1024

    const int STATUS_WAITING_FOR_MESSAGE = 0;
    const int STATUS_WAITING_FOR_URL_RESOLVING = 1;

    ssize_t received = 0;
    char temp_buffer[BUFFER_SIZE + 1];
    std::string buffer;
    int host_token_pos = -1;
    int status = STATUS_WAITING_FOR_MESSAGE;


};

class server_handler : public handler {
public:
    server_handler(int sock, proxy_server *serv) {
        this->fd = sock;
        this->serv = serv;
    }

    bool handle(epoll_event);
};

#endif //KIMBERLY_HANDLER_H
