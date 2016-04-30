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
#include "file_descriptor.h"
#include <sys/signalfd.h>
#include <signal.h>

class handler {
    friend class proxy_server;

    friend class client_handler;

    friend class hostname_resolver;

protected:
    file_descriptor fd;
    proxy_server *serv;
public:
    handler(int fd, proxy_server *serv) : fd(fd), serv(serv) { }

    handler(proxy_server *serv) : serv(serv) { }

    virtual ~handler() { }

    virtual void handle(const epoll_event &) = 0;

    virtual void disconnect() const {
        Log::d("disconnecting fd(" + inttostr(fd.get_fd()) + ")");
        serv->remove_handler(this);
        Log::d("disconnected successful");
    }
};

class notifier : public handler {
public:
    //TODO: take it's bodies out in .cpp file
    file_descriptor write_pipe;

    notifier(proxy_server *serv) : handler(serv) {
        int pipefds[2] = {};
        pipe(pipefds);
        fd.set_fd(pipefds[0]);
        write_pipe.set_fd(pipefds[1]);

        Log::d("write_pipe is " + inttostr(write_pipe.get_fd()) + ", read_pipe is " + inttostr(fd.get_fd()));

        // make read-end non-blocking
        setnonblocking(fd.get_fd());
    }

    virtual ~notifier() { }

    void handle(const epoll_event &) {
        char ch;
        Log::d("reading from fd " + inttostr(fd.get_fd()));
        read(fd.get_fd(), &ch, 1);
    }

    void notify() {
        Log::d("pre-notifying");
        char ch = 'x';
        write(write_pipe.get_fd(), &ch, 1);
        Log::d("successful notifying");
    }

    void disconnect() const {
        handler::disconnect();
    }

private:

};

class client_handler : public handler {
    friend class proxy_server;

    friend class hostname_resolver;

public:
    client_handler(int sock, proxy_server *serv) : handler(sock, serv) {
        message_len = -1;
        message_type = NOT_EVALUATED;
    }

    virtual ~client_handler() { }

    void handle(const epoll_event &);

private:
    buffer input_buffer;
    buffer output_buffer;
    int message_len;

    enum {
        NOT_EVALUATED, WITHOUT_BODY, VIA_CONTENT_LENGTH, VIA_TRANSFER_ENCODING, PRE_HTTPS_MODE, HTTPS_MODE
    } message_type;

    handler *clrh = 0;

    void resolve_host_ip(std::string, uint flags);

    void disconnect() const {
        if (clrh) {
            clrh->disconnect();
        }
        handler::disconnect();
    }

    // retunrs true if all the message was read
    bool read_message(handler *h, buffer &buf);

    class client_request_handler : public handler {
        friend class proxy_server;

    public:
        client_request_handler(int sock, proxy_server *serv, std::shared_ptr<client_handler> clh) : handler(sock,
                                                                                                            serv) {
            this->clh = clh;
            clh->clrh = this;
        }

        ~client_request_handler() {
            clh->clrh = 0;
        }

        void handle(const epoll_event &);

        void disconnect() const {
            clh->clrh = 0;
            handler::disconnect();
        }

    private:
        std::shared_ptr<client_handler> clh;
    };
};

class server_handler : public handler {
    friend class proxy_server;

public:
    server_handler(int sock, proxy_server *serv) : handler(sock, serv) { }

    virtual ~server_handler() { }

    void handle(const epoll_event &);
};

#endif //KIMBERLY_HANDLER_H