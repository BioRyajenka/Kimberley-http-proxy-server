//
// Created by root on 07.01.16.
//

#include "proxy_server.h"
#include "util.h"

#include <netdb.h>

proxy_server::proxy_server(std::string host, uint16_t port, int resolver_threads) {
    proxy_server::port = htons(port);
    proxy_server::host = inet_addr(host.c_str());

    if (proxy_server::host == uint32_t(-1)) {
        Log::fatal("Cannot assign requested address: " + host);
    }

    struct in_addr antelope;
    antelope.s_addr = proxy_server::host;
    Log::d("Server host is " + std::string(inet_ntoa(antelope)) + ", port is "
           + inttostr(ntohs(proxy_server::port)));

    CHK2(listenerSocket, socket(AF_INET, SOCK_STREAM, 0));

    int enable = 1;
    CHK(setsockopt(listenerSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)));

    Log::d("Main listener(fd=" + inttostr(listenerSocket) + ") created!");
    //setnonblocking(listenerSocket);

    CHK2(epfd, epoll_create(TARGET_CONNECTIONS));
    Log::d("Epoll(fd=" + inttostr(epfd) + ") created!");


    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = proxy_server::host;
    addr.sin_port = proxy_server::port;

    CHK(bind(listenerSocket, (struct sockaddr *) &addr, sizeof(addr)));
    Log::d("Listener binded to host");

    CHK(listen(listenerSocket, 10));
    Log::d("Start to listen host");

    add_handler(notifier_ = new notifier(this), EPOLLIN);
    add_handler(new server_handler(listenerSocket, this), EPOLLIN);
}

void proxy_server::loop() {
    Log::d("Start looping");
    static struct epoll_event events[TARGET_CONNECTIONS];
    while (1) {
        int epoll_events_count;
        CHK2(epoll_events_count, epoll_wait(epfd, events, TARGET_CONNECTIONS, -1));
        //Log::d("Epoll events count: " + inttostr(epoll_events_count)); // including notifier_fd

        clock_t tStart = clock();

        for (int i = 0; i < epoll_events_count; i++) {
            // Log::d("event " + inttostr(i) + ": " + eetostr(events[i]));
            if ((events[i].events & EPOLLERR) || events[i].events & EPOLLHUP) {
                close(events[i].data.fd);
            } else {
                int efd = events[i].data.fd;
                if (handlers[efd]) {
                    handlers[efd]->handle(events[i]);
                } else {
                    Log::e("Trying to handle fd(" + inttostr(efd) + ") which was deleted: " + eetostr(events[i]));
                }
            }
        }

        std::function<void()> to_run_function;
        while (to_run.peek(to_run_function)) {
            to_run_function();
        }

        //printf("Statistics: %d events handled at: %.2f second(s)\n", epoll_events_count,
        //       (double) (clock() - tStart) / CLOCKS_PER_SEC);
        std::cout.flush();
    }
}


void proxy_server::terminate() {
    for (auto &h : handlers) {
        if (h) close(h->fd);
    }

    close(listenerSocket);
    close(epfd);
}

void proxy_server::add_handler(handler *h, const uint &events) {
    //Log::d("Trying to insert handler to fd " + inttostr(fd));
    if (handlers.size() <= h->fd) {
        handlers.resize((size_t) h->fd + 1);
    }
    handlers[h->fd] = h;

    epoll_event e;
    e.data.fd = h->fd;
    e.events = events;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, h->fd, &e) < 0) {
        Log::e("Failed to insert handler to epoll [add]");
        perror(("add_handler (fd=" + inttostr(h->fd) + ")").c_str());
        std::cerr.flush();
    } else {
        Log::d("Handler was inserted to fd " + inttostr(h->fd));
    }
}

void proxy_server::modify_handler(int fd, uint events) {
    struct epoll_event e;

    if (fd >= handlers.size() || !handlers[fd]) {
        Log::e("Changing handler of unregistered file descriptor: " + inttostr(fd));
        Log::d("size is " + inttostr((int) handlers.size()) + ", handlers[fd] is " + (handlers[fd] ? "yes" : "no"));
        return;
    }

    e.data.fd = fd;
    e.events = events;

    if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &e) < 0) {
        Log::e("Failed to modify epoll events " + eetostr(e) + " [modify]");
        perror(("modify_handler (fd=" + inttostr(fd) + ", events=" + inttostr(events) + ")").c_str());
    }
}

void proxy_server::remove_handler(int fd) {
    if (fd >= handlers.size() || !handlers[fd]) {
        Log::e("Removing handler of a non registered file descriptor");
        return;
    }

    handlers[fd] = 0;

    Log::d("Removing fd(" + inttostr(fd) + ") handler");

    struct epoll_event e;
    e.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &e) < 0) {
        Log::e("Failed to modify epoll events [remove]");
        perror(("remove_handler (fd=" + inttostr(fd) + ")").c_str());
    }

    close(fd);

    Log::d("success");
}

/* * 1 * 2 * 3 * read-write functions * 3 * 2 * 1 * */

bool proxy_server::write_chunk(const handler &h, buffer &buf) {
    if (buf.empty()) {
        return false;
    }
    //Log::d("writing chunk");
    //Log::d("(" + inttostr(buf.length()) + "): " + buf.string_data());
    int kkek = buf.length();

    int to_send = std::min(buf.length(), BUFFER_SIZE);
    //Log::d(inttostr(to_send) + " bytes sended");
    if (send(h.fd, buf.get(to_send), (size_t) to_send, 0) != to_send) {
        Log::fatal("Error writing to socket");
        //exit(-1);
    }
    //Log::d("(" + inttostr(buf.length()) + "): " + buf.string_data());
    //Log::d(buf.empty() ? "empty" : "not empty");

    //Log::d("write_chunk(" + inttostr(to_send) + ", " + inttostr(kkek) + ")");

    return buf.empty();
}

bool proxy_server::read_chunk(const handler &h, buffer *buf) {
    ssize_t received;
    if ((received = recv(h.fd, temp_buffer, BUFFER_SIZE, 0)) < 0) {
        perror("read_chunk");
        Log::fatal("fatal in read_chunk");
        //exit(-1);
    }
    if (buf != nullptr) {
        buf->put(temp_buffer, (size_t) received);
    }

    Log::d("read_chunk (" + inttostr((int) received) + "): \n\"" + std::string(temp_buffer, received) + "\"");

    return received == 0;
}

void proxy_server::notify_epoll() {
    notifier_->notify();
}

void proxy_server::add_resolver_task(client_handler *h, std::string hostname, const uint &flags) {
    hostname_resolve_queue.push([this, h, &hostname, flags]() {
        Log::d("Resolving hostname \"" + hostname + "\"");
        uint16_t port = 80;

        for (size_t i = 0; i < hostname.length(); i++) {
            if (hostname[i] == ':') {
                port = (uint16_t) strtoint(hostname.substr(i + 1));
                hostname = hostname.substr(0, i);
                break;
            }
        }

        Log::d("hostname is " + hostname + ", port is " + inttostr(port));

        struct hostent *he;

        if ((he = gethostbyname(hostname.c_str())) == 0) {
            Log::d("hostname " + hostname + " cannot be resolved");
            to_run.push([this, h]() {
                h->output_buffer.set("HTTP/1.1 404 Not Found\r\nContent-Length: 26\r\n\r\n<html>404 not found</html>");
                modify_handler(h->fd, EPOLLOUT);
            });
            return;
        }

        for (int i = 0; (struct in_addr **) he->h_addr_list[i] != NULL; i++) {
            Log::d("ips[" + inttostr(i) + "] = " + std::string(inet_ntoa(*((struct in_addr *) he->h_addr_list[i]))));
        }

        Log::d("Ip is " + std::string(inet_ntoa(*((struct in_addr *) he->h_addr_list[0]))));

        to_run.push([h, he, port, this, flags]() {
            CHK2(h->_client_request_socket, socket(AF_INET, SOCK_STREAM, 0));

            setnonblocking(h->_client_request_socket);

            struct sockaddr_in addr;

            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *) he->h_addr_list[0])));
            addr.sin_port = htons(port);

            connect(h->_client_request_socket, (struct sockaddr *) &addr, sizeof(addr));
            if (errno != EINPROGRESS) {
                perror("connect");
                Log::fatal("connect");
            }

            Log::d("Creating client_request socket fd(" + inttostr(h->_client_request_socket) + ") for client fd(" +
                   inttostr(h->fd) +
                   ")");
            add_handler(new client_handler::client_request_handler(h->_client_request_socket, this, h), flags);
        });
    });
}