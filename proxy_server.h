//
// Created by root on 07.01.16.
//

#ifndef KIMBERLY_PROXY_SERVER_H
#define KIMBERLY_PROXY_SERVER_H

#include <string>
#include <vector>
#include <arpa/inet.h>
#include <mutex>
#include "buffer.h"
#include "concurrent_queue.h"

#include "hostname_resolver.h"

class handler;

class client_handler;

class notifier;

class proxy_server {
#define BUFFER_SIZE 1024
#define TARGET_CONNECTIONS 10000
#define DEFAULT_RESOLVER_THREADS 100

    friend class handler;

    friend class notifier;

    friend class client_handler;

    friend class server_handler;

    friend class hostname_resolver;

public:
    proxy_server(std::string host, uint16_t port, int resolver_threads = DEFAULT_RESOLVER_THREADS);

    void loop();

    void terminate();

protected:
    void add_handler(handler *h, const uint &events);

    void modify_handler(int fd, uint events);

    void remove_handler(int fd);

    // returns true if large_buffer was totally sended to fd
    bool write_chunk(const handler &h, buffer &buf);

    // returns true if recv returned 0
    bool read_chunk(const handler &h, buffer *buf);

protected:
    void add_resolver_task(client_handler *h, std::string hostname, uint flags);

    void notify_epoll();

private:
    uint16_t port;
    in_addr_t host;
    int listenerSocket;

    int epfd;//main epoll

    std::vector<handler *> handlers;
    concurrent_queue<std::function<void()>> hostname_resolve_queue;
    concurrent_queue<std::function<void()>> to_run;

    char temp_buffer[BUFFER_SIZE + 1];

    notifier *notifier_;
};

#endif //KIMBERLY_PROXY_SERVER_H