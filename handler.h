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
#include "buffer.h"

class handler {
public:
    int fd;
    proxy_server *serv;
public:
    virtual bool handle(epoll_event) = 0;

    void disconnect() const {
        serv->remove_handler(fd);
    }
};

class client_handler : public handler {
public:
    client_handler(int sock, proxy_server *serv) {
        this->fd = sock;
        this->serv = serv;
        message_len = -1;
        message_type = NOT_EVALUATED;
    }

    bool handle(epoll_event);

    void process();

private:
    buffer input_buffer;
    buffer output_buffer;
    int message_len;

    enum {NOT_EVALUATED, WITHOUT_BODY, VIA_CONTENT_LENGTH, VIA_TRANSFER_ENCODING, HTTPS_MODE} message_type;

    int _client_request_socket;
    handler *clrh = 0;

    void resolve_host_ip(std::string);

    // retunrs true if all the message was read
    bool read_message(const handler &h, buffer &buf);

    void disconnect() const {
        handler::disconnect();
        if (clrh) {
            clrh->disconnect();
        }
    }

    class client_request_handler : public handler {
    public:
        client_request_handler(int sock, proxy_server *serv, client_handler *clh) {
            this->fd = sock;
            this->serv = serv;
            this->clh = clh;

            clh->clrh = this;
        }

        bool handle(epoll_event);

        void disconnect() const {
            clh->clrh = 0;
            handler::disconnect();
        }

    private:
        client_handler *clh;
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