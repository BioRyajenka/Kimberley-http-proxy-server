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

class client_handler : public handler {
public:
    client_handler(int sock, proxy_server *serv) {
        this->fd = sock;
        this->serv = serv;
    }

    bool handle(epoll_event);

    void process();

private:
#define BUFFER_SIZE 1024
    const int STATUS_WAITING_FOR_MESSAGE = 0;
    const int STATUS_WAITING_FOR_IP_RESOLVING = 1;
    const int STATUS_WAITING_FOR_CREATING_REQUEST_HANDLER = 2;
    const int STATUS_WAITING_FOR_HOST_ANSWER = 3;
    const int STATUS_WRITING_HOST_ANSWER = 4;

    char buffer[BUFFER_SIZE + 1];
    std::string large_buffer;
    int bytes_sended;

    int status = STATUS_WAITING_FOR_MESSAGE;
    int _client_request_socket;

    void resolve_host_ip(std::string);

    class client_request_handler : public handler {
    public:
        client_request_handler(int sock, proxy_server *serv, client_handler *clh) {
            this->fd = sock;
            this->serv = serv;
            this->clh = clh;
        }

        bool handle(epoll_event);

    private:
        const int STATUS_WAITING_FOR_EPOLLOUT = 0;
        const int STATUS_WAITING_FOR_ANSWER = 1;
        int status = STATUS_WAITING_FOR_EPOLLOUT;
        client_handler *clh;
        int response_len;
    };
};

class server_handler : public handler {
public:
    //TODO: inherit constructors
    server_handler(int sock, proxy_server *serv) {
        this->fd = sock;
        this->serv = serv;
    }

    bool handle(epoll_event);
};

#endif //KIMBERLY_HANDLER_H