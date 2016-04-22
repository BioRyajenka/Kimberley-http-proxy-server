//
// Created by root on 07.01.16.
//

#include "proxy_server.h"
#include "handler.h"
#include "util.h"

#include <netdb.h>

void proxy_server::run_all_toruns() {
    std::function<void()> to_run_function;
    while (to_run.peek(to_run_function)) {
        Log::d("successful picking");
        to_run_function();
    }
}

proxy_server::~proxy_server() {
    /*
     * Strict in this order
     * stopping_threads
     * to_run
     * deleting h's
     * to_delete
     */

    // releasing waiters. necessary to call it before deleting handlers
    hostname_resolve_queue.release_waiters();

    for (auto hr : hostname_resolvers) {
        delete hr;
    }

    // I'm calling it here because there may be memory freeing
    run_all_toruns();

    // if I'll write &h, valgrind will abuse
    for (auto h : handlers) {
        if (h) {
            Log::d("!deleting fd(" + inttostr(h->fd) + ")");
            h->disconnect();
        }
    }

    // not necessary, actually
    to_free.clear();

    close(listenerSocket);
    close(epfd);
}

proxy_server::proxy_server(std::string host, uint16_t port, int resolver_threads) {
    proxy_server::port = htons(port);
    proxy_server::host = inet_addr(host.c_str());

    if (proxy_server::host == uint32_t(-1)) {
        Log::fatal("Cannot assign requested address: " + host);
    }

    struct in_addr host_in_addr;
    host_in_addr.s_addr = proxy_server::host;
    Log::d("Server host is " + std::string(inet_ntoa(host_in_addr)) + ", port is "
           + inttostr(ntohs(proxy_server::port)));

    if ((listenerSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        Log::fatal("fatal");
    }

    int enable = 1;
    if (setsockopt(listenerSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt");
        Log::fatal("fatal");
    }

    Log::d("Main listener(fd=" + inttostr(listenerSocket) + ") created!");
    //setnonblocking(listenerSocket);

    if ((epfd = epoll_create(TARGET_CONNECTIONS)) < 0) {
        perror("epoll_create");
        Log::fatal("fatal");
    }
    Log::d("Epoll(fd=" + inttostr(epfd) + ") created!");

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = proxy_server::host;
    addr.sin_port = proxy_server::port;

    if (bind(listenerSocket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("bind");
        Log::fatal("fatal");
    }
    Log::d("Listener binded to host");

    if (listen(listenerSocket, 10) < 0) {
        perror("listen");
        Log::fatal("fatal");
    }

    Log::d("Start to listen host");

    add_handler(notifier_ = std::make_shared<notifier>(this), EPOLLIN);
    add_handler(std::make_shared<server_handler>(listenerSocket, this), EPOLLIN);

    for (int i = 0; i < resolver_threads; i++) {
        hostname_resolvers.push_back(new hostname_resolver(this));
        hostname_resolvers.back()->start();
    }

    Log::d("Main thread id: " + inttostr((int) pthread_self()));
}

void proxy_server::loop() {
    Log::d("Start looping");
    static struct epoll_event events[TARGET_CONNECTIONS];
    while (!terminating) {
        int epoll_events_count;
        if ((epoll_events_count = epoll_wait(epfd, events, TARGET_CONNECTIONS, -1)) < 0) {
            if (errno != EINTR) {
                perror("epoll_wait");
                Log::fatal("fatal");
            } else {
                Log::e("EINTR");
                return;
            }
        }
        //Log::d("Epoll events count: " + inttostr(epoll_events_count)); // including notifier_fd

        //clock_t tStart = clock();

        for (int i = 0; i < epoll_events_count; i++) {
            // Log::d("event " + inttostr(i) + ": " + eetostr(events[i]));
            if ((events[i].events & EPOLLERR) || events[i].events & EPOLLHUP) {
                close(events[i].data.fd);
            } else {
                int efd = events[i].data.fd;
                if (handlers[efd]) {
                    handlers[efd]->handle(events[i]);
                } else {
                    // actually, it's norm situation.
                    // it happens when more earlier EE commits disconnection of more later one
                    Log::w("Trying to handle fd(" + inttostr(efd) + ") which was deleted: " + eetostr(events[i]));
                }
            }
        }

        run_all_toruns();

        to_free.clear();

        //printf("Statistics: %d events handled at: %.2f second(s)\n", epoll_events_count,
        //       (double) (clock() - tStart) / CLOCKS_PER_SEC);
        std::cout.flush();
    }
}

void proxy_server::add_handler(std::shared_ptr<handler> h, const uint &events) {
    //Log::d("Trying to insert handler to fd " + inttostr(fd));
    if (handlers.size() <= (ulong) h->fd) {
        handlers.resize((size_t) h->fd + 1);
    }

    if (handlers[h->fd]) {
        Log::e("adding handler fd(" + inttostr(h->fd) + ") to engaged space");
        to_free.push_back(handlers[h->fd]);
    }

    handlers[h->fd] = h;

    epoll_event e = {};
    e.data.fd = h->fd;
    e.events = events;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, h->fd, &e) < 0) {
        Log::e("Failed to insert handler to epoll");
        perror(("add_handler (fd=" + inttostr(h->fd) + ")").c_str());
    } else {
        Log::d("Handler was inserted to fd " + inttostr(h->fd) + " with flags " + eeflagstostr(events));
    }
}

void proxy_server::modify_handler(int fd, uint events) {
    struct epoll_event e;


    if ((ulong) fd >= handlers.size() || !handlers[fd]) {
        Log::e("Changing handler of unregistered file descriptor: " + inttostr(fd));
        Log::d("size is " + inttostr((int) handlers.size()) + ", handlers[fd] is " + (handlers[fd] ? "yes" : "no"));
        return;
    }
    e = {};
    e.data.fd = fd;
    e.events = events;

    if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &e) < 0) {
        Log::e("Failed to modify epoll event " + eetostr(e));
        perror(("modify_handler (fd=" + inttostr(fd) + ", events=" + inttostr(events) + ")").c_str());
    }
}

void proxy_server::remove_handler(int fd) {
    Log::d("Removing fd(" + inttostr(fd) + ") handler");
    if ((ulong) fd >= handlers.size() || !handlers[fd]) {
        //this situation may occur when this handler was misplaced from handlers by another one and this another was terminated
        Log::e("Removing handler of unregistered file descriptor");
        return;
    }

    to_free.push_back(handlers[fd]);
    handlers[fd] = 0;

    struct epoll_event e;
    e = {};
    e.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &e) < 0) {
        Log::e("Failed to remove epoll event");
        perror(("remove_handler (fd=" + inttostr(fd) + ")").c_str());
    }

    close(fd);

    Log::d("success");
}

void proxy_server::terminate() {
    Log::d("terminating!");
    terminating = true;
    notify_epoll();
}

bool proxy_server::read_chunk(handler *h, buffer *buf) {
    ssize_t received;
    if ((received = recv(h->fd, temp_buffer, BUFFER_SIZE, 0)) < 0) {
        Log::e("Error reading from socket");
        perror("perror:");
        //exit(-1);
    }
    if (buf != nullptr) {
        buf->put(temp_buffer, (size_t) received);
    }

    Log::d("read_chunk (" + inttostr((int) received) + "): \n\"" + std::string(temp_buffer, (ulong) received) + "\"");

    return received == 0;
}

void proxy_server::notify_epoll() {
    notifier_->notify();
}

/* * 1 * 2 * 3 * read-write functions * 3 * 2 * 1 * */

bool proxy_server::write_chunk(handler *h, buffer &buf) {
    if (buf.empty()) {
        return false;
    }
    Log::d("writing chunk: \"\n" + buf.string_data() + "\"");

    int to_send = std::min(buf.length(), BUFFER_SIZE);
    if (send(h->fd, buf.get(to_send), (size_t) to_send, 0) != to_send) {
        Log::e("Error writing to socket. Disconnecting.");
        perror("perror:");
        buf.stash();//not necessary
        h->disconnect();
        return false;
    }

    return buf.empty();
}

void proxy_server::add_resolver_task(int fd, std::string hostname, uint flags) {
    std::shared_ptr<client_handler> h = std::dynamic_pointer_cast<client_handler, handler>(handlers[fd]);
    hostname_resolve_queue.push([this, h, hostname, flags]() {
        Log::d("RESOLVER: \tResolving hostname \"" + hostname + "\"");
        uint16_t port = 80;

        ulong pos = hostname.find(":");
        std::string new_hostname = hostname.substr(0, pos);
        if (pos != std::string::npos) {
            port = (uint16_t) strtoint(hostname.substr(pos + 1));
        }

        Log::d("RESOLVER: \thostname is " + new_hostname + ", port is " + inttostr(port));

        struct addrinfo hints, *servinfo;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(new_hostname.c_str(), inttostr(port).c_str(), &hints, &servinfo) != 0) {
            Log::d("RESOLVER: \thostname " + new_hostname + " cannot be resolved");
            to_run.push([this, h]() {
                h->output_buffer.set("HTTP/1.1 404 Not Found\r\nContent-Length: 26\r\n\r\n<html>404 not found</html>");
                modify_handler(h->fd, EPOLLOUT);
            });
            return;
        }

        to_run.push([h, servinfo, this, flags, new_hostname]() {
            int client_request_socket = 0;
            struct addrinfo *p;
            // loop through all the results and connect to the first we can
            for (p = servinfo; p != NULL; p = p->ai_next) {
                if ((client_request_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                    perror("socket");
                    continue;
                }

                setnonblocking(client_request_socket);

                Log::d("trying to connect to smth");

                if (!connect(client_request_socket, p->ai_addr, p->ai_addrlen)) {
                    //connected immediately
                    break;
                }
                if (errno == EINTR) {
                    close(client_request_socket);
                    break;
                }
                if (errno != EINPROGRESS) {
                    perror("connect");
                    close(client_request_socket);
                    continue;
                }
                //checking timeout
                fd_set wset;
                struct timeval timeout;
                FD_ZERO(&wset);
                FD_SET(client_request_socket, &wset);
                timeout.tv_sec = DEFAULT_SECONDS_TIMEOUT;
                timeout.tv_usec = 0;

                int select_retval = select(FD_SETSIZE, 0, &wset, 0, &timeout);
                if (select_retval == 0) {
                    Log::d("timeout expires");
                    close(client_request_socket);
                    continue;
                }
                if (select_retval == -1) {
                    perror("select");
                    close(client_request_socket);
                    continue;
                }

                break; // if we get here, we must have connected successfully
            }

            if (p == NULL) {
                Log::d("Can't connect");
                h->output_buffer.set(
                        "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 36\r\n\r\n<html>503 Service Unavailable</html>");
                modify_handler(h->fd, EPOLLOUT);
                freeaddrinfo(servinfo);
                return;
            }

            Log::d("succeed connecting");

            freeaddrinfo(servinfo); // all done with this structure

            Log::d("Creating client_request socket fd(" + inttostr(client_request_socket) + ") for client fd(" +
                   inttostr(h->fd) + ")");
            add_handler(std::make_shared<client_handler::client_request_handler>(client_request_socket, this, h),
                        flags);
        });
        Log::d("RESOLVER: \tproxy_server.cpp:add_resolver_task");
    });
}