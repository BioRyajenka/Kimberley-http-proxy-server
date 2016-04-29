//
// Created by jackson on 14.04.16.
//

#include <stddef.h>
#include <thread>
#include <stdlib.h>
#include "hostname_resolver.h"
#include "util.h"
#include "handler.h"
#include "util.h"

#include <netdb.h>


hostname_resolver::~hostname_resolver() {
    if (!serv->hostname_resolve_queue.is_releasing()) {
        Log::fatal("hostname_resolve_queue should be released first");
    }
    thread->join();
    delete thread;
}

void hostname_resolver::start() {
    thread = new std::thread((std::function<void()>) ([this] {
        Log::d("resolver thread id: " + inttostr((int) pthread_self()));
        while (1) {
            //Log::d(std::string("serv is ") + (serv == nullptr ? "null" : "not null"));
            std::function<void()> func;
            if (!serv->hostname_resolve_queue.pop(func)) {
                return;
            }
            //Log::d("VALUE IS " + inttostr(func));
            func();
            Log::d("RESOLVER: \tfunction successfully executed");

            serv->notify_epoll();
        }
    }));
}

void hostname_resolver::add_task(proxy_server *serv, std::shared_ptr<client_handler> h, std::string hostname,
                                 uint flags) {
    serv->hostname_resolve_queue.push([serv, h, hostname, flags]() {
        Log::d("RESOLVER: \tResolving hostname \"" + hostname + "\"");
        uint16_t port = 80;

        ulong pos = hostname.find(":");
        std::string new_hostname = hostname.substr(0, pos);
        if (pos != std::string::npos) {
            port = (uint16_t) strtoint(hostname.substr(pos + 1));
        }

        Log::d("RESOLVER: \thostname is " + new_hostname + ", port is " + inttostr(port));

        struct addrinfo *servinfo;

        if (cashed_hostnames.find(new_hostname) != cashed_hostnames.end()) {
            Log::d("taking " + new_hostname + " from cash");
            servinfo = cashed_hostnames[new_hostname];
        } else {
            Log::d(new_hostname + " wasn't found in cash");
            struct addrinfo hints;
            memset(&hints, 0, sizeof hints);
            hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
            hints.ai_socktype = SOCK_STREAM;

            if (getaddrinfo(new_hostname.c_str(), inttostr(port).c_str(), &hints, &servinfo) != 0) {
                Log::d("RESOLVER: \thostname " + new_hostname + " cannot be resolved");
                serv->to_run.push([serv, h]() {
                    h->output_buffer.set(
                            "HTTP/1.1 404 Not Found\r\nContent-Length: 26\r\n\r\n<html>404 not found</html>");
                    serv->modify_handler(h->fd, EPOLLOUT);
                });
                return;
            }
            std::unique_lock<std::mutex> lock(cashing_mutex);
            if (cashed_hostnames.find(new_hostname) != cashed_hostnames.end()) {
                freeaddrinfo(servinfo);
                servinfo = cashed_hostnames[new_hostname];//TODO: use iterator here
            } else {
                cashed_hostnames[new_hostname] = servinfo;//TODO: use iterator here
            }
        }

        // trying to connect [thread-safely]

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
            // freeaddrinfo(servinfo);
            serv->to_run.push([h, serv, new_hostname]() {
                Log::d("Can't connect to " + new_hostname);
                h->output_buffer.set(
                        "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 36\r\n\r\n<html>503 Service Unavailable</html>");
                serv->modify_handler(h->fd, EPOLLOUT);
            });
        } else {
            // freeaddrinfo(servinfo); // all done with this structure
            serv->to_run.push([h, client_request_socket, serv, flags]() {
                Log::d("succeed connecting");

                Log::d("Creating client_request socket fd(" + inttostr(client_request_socket) + ") for client fd(" +
                       inttostr(h->fd) + ")");
                serv->add_handler(
                        std::make_shared<client_handler::client_request_handler>(client_request_socket, serv, h),
                        flags);
            });
        }
        Log::d("RESOLVER: \tproxy_server.cpp:add_resolver_task");
    });
}

int hostname_resolver::free_id = 0;
std::map<std::string, struct addrinfo*> hostname_resolver::cashed_hostnames;
std::mutex hostname_resolver::cashing_mutex;