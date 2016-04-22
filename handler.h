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
#include <sys/signalfd.h>
#include <signal.h>

class handler {
    friend class proxy_server;

    friend class client_handler;

protected:
    int fd;
    proxy_server *serv;
public:
    virtual void handle(const epoll_event &) = 0;
    virtual ~handler() {}

    virtual void disconnect() const {
        Log::d("disconnecting fd(" + inttostr(fd) + ")");
        serv->remove_handler(fd);
        Log::d("disconnected successful");
    }
};

class notifier : public handler {
public:
    //TODO: take it's bodies out in .cpp file
    int pipefds[2];

    int read_pipe, write_pipe;

    notifier(proxy_server *serv) {
        this->serv = serv;
        pipefds[2] = {};
        pipe(pipefds);
        read_pipe = pipefds[0];
        write_pipe = pipefds[1];

        Log::d("write_pipe is " + inttostr(write_pipe) + ", read_pipe is " + inttostr(read_pipe));

        // make read-end non-blocking
        int flags = fcntl(read_pipe, F_GETFL, 0);
        fcntl(write_pipe, F_SETFL, flags|O_NONBLOCK);

        // add the read end to the epoll
        fd = read_pipe;

        Log::d("write_pipe2 is " + inttostr(write_pipe));
    }
    virtual ~notifier() {}

    void handle(const epoll_event &) {
        int result;
        char ch;
        Log::d("reading from fd " + inttostr(read_pipe));
        //read_pipe = 5;
        result = read(read_pipe, &ch, 1);
    }

    void notify() {
        Log::d("pre-notifying");
        write_pipe = 6;
        Log::d("notifying to fd " + inttostr(write_pipe));
        if (write_pipe != 6) {
            Log::e("WRITE PIPE NE RAVNO SHESTI!");
        }
        char ch = 'x';
        //write_pipe = 6;
        write(write_pipe, &ch, 1);
        Log::d("successful notifying");
    }

    void disconnect() const {
        handler::disconnect();
        close(read_pipe);
        close(write_pipe);
    }

private:

};

class client_handler : public handler {
    friend class proxy_server;

public:
    client_handler(int sock, proxy_server *serv) {
        this->fd = sock;
        this->serv = serv;
        message_len = -1;
        message_type = NOT_EVALUATED;
    }
    virtual ~client_handler() {}

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
        handler::disconnect();
        if (clrh) {
            clrh->disconnect();
        }
    }

    // retunrs true if all the message was read
    bool read_message(handler *h, buffer &buf);

    class client_request_handler : public handler {
        friend class proxy_server;

    public:
        client_request_handler(int sock, proxy_server *serv, std::shared_ptr<client_handler> clh) {
            this->fd = sock;
            this->serv = serv;
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

        bool deleteme = false;
    };
};

class server_handler : public handler {
    friend class proxy_server;
public:
    //TODO: inherit constructors
    server_handler(int sock, proxy_server *serv) {
        this->fd = sock;
        this->serv = serv;
    }

    virtual ~server_handler() {}

    void handle(const epoll_event &);
};

#endif //KIMBERLY_HANDLER_H