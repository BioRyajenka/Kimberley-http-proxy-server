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

    virtual void disconnect() const {
        std::cout << "disconnecting fd(" << fd << ")" << "\n";
        serv->remove_handler(fd);
        std::cout << "success disconnecting\n";
    }
};

class notifier : public handler {
public:
    notifier(proxy_server *serv) {
        main_thread = pthread_self();
        sigset_t sigmask;
        sigemptyset(&sigmask);
        sigaddset(&sigmask, SIGUSR1);

        sigprocmask(SIG_BLOCK, &sigmask, NULL);

        fd = signalfd(-1, &sigmask, 0);
    }

    void handle(const epoll_event &) {
        //just to clear it out

    }

    void notify() {
        pthread_kill(main_thread, SIGUSR1);
    }

    void disconnect() const {
        handler::disconnect();
    }

private:
    pthread_t main_thread;
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

    void handle(const epoll_event &);

private:
    buffer input_buffer;
    buffer output_buffer;
    int message_len;

    enum {
        NOT_EVALUATED, WITHOUT_BODY, VIA_CONTENT_LENGTH, VIA_TRANSFER_ENCODING, PRE_HTTPS_MODE, HTTPS_MODE
    } message_type;

    int _client_request_socket;
    handler *clrh = 0;

    void resolve_host_ip(std::string, const uint &flags);

    // retunrs true if all the message was read
    bool read_message(const handler &h, buffer &buf);

    void disconnect() const {
        handler::disconnect();
        if (clrh) {
            clrh->disconnect();
        }
    }

    class client_request_handler : public handler {
        friend class proxy_server;

    protected:
        client_request_handler(int sock, proxy_server *serv, client_handler *clh) {
            this->fd = sock;
            this->serv = serv;
            this->clh = clh;

            clh->clrh = this;
        }

        void handle(const epoll_event &);

        void disconnect() const {
            clh->clrh = 0;
            handler::disconnect();
        }

    private:
        client_handler *clh;
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

    void handle(const epoll_event &);
};

#endif //KIMBERLY_HANDLER_H