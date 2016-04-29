//
// Created by root on 07.01.16.
//

#include <netdb.h>
#include "proxy_server.h"
#include "handler.h"
#include "my_addrinfo.h"

void proxy_server::run_all_toruns() {
    std::function<void()> to_run_function;
    while (to_run.peek(to_run_function)) {
        Log::d("successful picking");
        to_run_function();
    }
}

proxy_server::proxy_server(std::string hostname, uint16_t port_number, int resolver_threads) {
    uint16_t port = htons(port_number);
    in_addr_t host = inet_addr(hostname.c_str());

    if (host == uint32_t(-1)) {
        Log::fatal("Cannot assign requested address: " + host);
    }

    struct in_addr host_in_addr;
    host_in_addr.s_addr = host;
    Log::d("Server host is " + std::string(inet_ntoa(host_in_addr)) + ", port is " + inttostr(ntohs(port)));

    int listener_socket;
    if ((listener_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        Log::fatal("fatal");
    }

    int enable = 1;
    if (setsockopt(listener_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        close(listener_socket);
        perror("setsockopt");
        Log::fatal("fatal");
    }

    Log::d("Main listener(fd=" + inttostr(listener_socket) + ") created!");

    int _epfd;
    if ((_epfd = epoll_create(TARGET_CONNECTIONS)) < 0) {
        close(listener_socket);
        perror("epoll_create");
        Log::fatal("fatal");
    }
    epfd.set_fd(_epfd);
    Log::d("Epoll(fd=" + inttostr(_epfd) + ") created!");

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = host;
    addr.sin_port = port;

    if (bind(listener_socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(listener_socket);
        perror("bind");
        Log::fatal("fatal");
    }
    Log::d("Listener binded to host");

    if (listen(listener_socket, 100) < 0) {
        close(listener_socket);
        perror("listen");
        Log::fatal("fatal");
    }

    Log::d("Start to listen host");

    add_handler(notifier_ = std::make_shared<notifier>(this), EPOLLIN);

    add_handler(std::make_shared<server_handler>(listener_socket, this), EPOLLIN);

    resolver = std::make_shared<hostname_resolver>(resolver_threads);

    Log::d("Main thread id: " + inttostr((int) pthread_self()));
}

void proxy_server::loop() {
    Log::d("Start looping");
    static struct epoll_event events[TARGET_CONNECTIONS];
    while (!terminating) {
        int epoll_events_count;
        if ((epoll_events_count = epoll_wait(epfd.get_fd(), events, TARGET_CONNECTIONS, -1)) < 0) {
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
            int efd = events[i].data.fd;
            // Log::d("event " + inttostr(i) + ": " + eetostr(events[i]));
            if ((events[i].events & EPOLLERR) || events[i].events & EPOLLHUP) {
                handlers[efd]->disconnect();
            } else {
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
    }
}


void proxy_server::add_handler(std::shared_ptr<handler> h, const uint &events) {
    //Log::d("Trying to insert handler to fd " + inttostr(fd));
    if (handlers.size() <= (ulong) h->fd.get_fd()) {
        handlers.resize((size_t) h->fd.get_fd() + 1);
    }

    if (handlers[h->fd.get_fd()]) {
        Log::e("adding handler fd(" + inttostr(h->fd.get_fd()) + ") to engaged space");
        // h will free automatically
    }

    handlers[h->fd.get_fd()] = h;

    epoll_event e = {};
    e.data.fd = h->fd.get_fd();
    e.events = events;

    if (epoll_ctl(epfd.get_fd(), EPOLL_CTL_ADD, h->fd.get_fd(), &e) < 0) {
        Log::e("Failed to insert handler to epoll");
        perror(("add_handler (fd " + inttostr(h->fd.get_fd()) + ")").c_str());
    } else {
        Log::d("Handler was inserted to fd (" + inttostr(h->fd.get_fd()) + ") with flags " + eeflagstostr(events));
    }
}

void proxy_server::modify_handler(handler *h, uint events) {
    struct epoll_event e;

    int fd = h->fd.get_fd();

    if ((ulong) fd >= handlers.size() || !handlers[fd]) {
        Log::e("Changing handler of unregistered file descriptor: " + inttostr(fd));
        Log::d("size is " + inttostr((int) handlers.size()) + ", handlers[fd] is " + (handlers[fd] ? "yes" : "no"));
        return;
    }
    e = {};
    e.data.fd = fd;
    e.events = events;

    if (epoll_ctl(epfd.get_fd(), EPOLL_CTL_MOD, fd, &e) < 0) {
        Log::e("Failed to modify epoll event " + eetostr(e));
        perror(("modify_handler (fd " + inttostr(fd) + ", events=" + inttostr(events) + ")").c_str());
    }
}

void proxy_server::remove_handler(const handler *h) {
    int fd = h->fd.get_fd();
    Log::d("Removing fd(" + inttostr(fd) + ") handler");
    if ((ulong) fd >= handlers.size() || !handlers[fd]) {
        //this situation may occur when this handler was misplaced from handlers by another one and this another was terminated
        //or, e.g., when epoll says fd should be closed and also associated client_handler disconnects
        Log::w("Removing handler of unregistered file descriptor");
        return;
    }

    handlers[fd] = 0;

    struct epoll_event e;
    e = {};
    e.data.fd = fd;
    if (epoll_ctl(epfd.get_fd(), EPOLL_CTL_DEL, fd, &e) < 0) {
        Log::e("Failed to remove epoll event");
        perror(("remove_handler (fd " + inttostr(fd) + ")").c_str());
    }

    close(fd);

    Log::d("success");
}

void proxy_server::terminate() {
    Log::d("terminating!");
    terminating = true;
    notifier_->notify();
}

bool proxy_server::read_chunk(handler *h, buffer *buf) {
    ssize_t received;
    if ((received = recv(h->fd.get_fd(), temp_buffer, BUFFER_SIZE, 0)) < 0) {
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

/* * 1 * 2 * 3 * read-write functions * 3 * 2 * 1 * */

bool proxy_server::write_chunk(handler *h, buffer &buf) {
    if (buf.empty()) {
        return false;
    }
    Log::d("writing chunk: \"\n" + buf.string_data() + "\"");

    int to_send = std::min(buf.length(), BUFFER_SIZE);
    if (send(h->fd.get_fd(), buf.get(to_send), (size_t) to_send, 0) != to_send) {
        Log::e("Error writing to socket. Disconnecting.");
        perror("perror:");
        buf.stash();//not necessary
        h->disconnect();
        return false;
    }

    return buf.empty();
}

void proxy_server::add_resolver_task(client_handler *h, std::string hostname, uint flags) {
    resolver->add_task(hostname, [this, h, hostname, flags](my_addrinfo *servinfo) {
        int client_request_socket = 0;
        struct addrinfo *p;
        // loop through all the results and connect to the first we can
        for (p = servinfo->get_info(); p != NULL; p = p->ai_next) {
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
            timeout.tv_sec = CONNECTION_TIMEOUT;
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
            to_run.push([h, this, hostname]() {
                Log::d("Can't connect to " + hostname);
                h->output_buffer.set("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 36\r\n\r\n"
                                             "<html>503 Service Unavailable</html>");
                modify_handler(h, EPOLLOUT);
            });
        } else {
            to_run.push([h, client_request_socket, this, flags]() {
                Log::d("connection successful");

                Log::d("Creating client_request socket fd(" + inttostr(client_request_socket) + ") for client fd(" +
                       inttostr(h->fd.get_fd()) + ")");
                add_handler(std::make_shared<client_handler::client_request_handler>(client_request_socket, this, h),
                            flags);
            });
        }

        notifier_->notify();
        Log::d("RESOLVER: \tproxy_server.cpp:add_resolver_task");
    }, [this, hostname, h]() {
        Log::d("RESOLVER: \thostname " + hostname + " cannot be resolved");
        to_run.push([this, h]() {
            h->output_buffer.set("HTTP/1.1 404 Not Found\r\nContent-Length: 26\r\n\r\n<html>404 not found</html>");
            modify_handler(h, EPOLLOUT);
        });
    });
}