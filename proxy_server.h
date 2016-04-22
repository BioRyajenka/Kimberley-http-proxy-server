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
#define TARGET_CONNECTIONS 1000
#define DEFAULT_RESOLVER_THREADS 1 //20

    friend class handler;

    friend class notifier;

    friend class client_handler;

    friend class server_handler;

    friend class hostname_resolver;

public:
    proxy_server(std::string host, uint16_t port, int resolver_threads = DEFAULT_RESOLVER_THREADS);

    ~proxy_server();

    void loop();

    void terminate();

protected:
    void add_handler(std::shared_ptr<handler> h, const uint &events);

    void modify_handler(int fd, uint events);

    void remove_handler(int fd);

    // returns true if large_buffer was totally sended to fd
    bool write_chunk(handler *h, buffer &buf);

    // returns true if recv returned 0
    bool read_chunk(handler *h, buffer *buf);

    void notify_epoll();

    void add_resolver_task(int fd, std::string hostname, uint flags);

private:
    void run_all_toruns();

    uint16_t port;
    in_addr_t host;
    int listenerSocket;

    int epfd;//main epoll

    std::vector<std::shared_ptr<handler>> handlers;
    std::vector<hostname_resolver *> hostname_resolvers;

    concurrent_queue<std::function<void()>> hostname_resolve_queue;
    concurrent_queue<std::function<void()>> to_run;

    std::vector<std::shared_ptr<handler>> to_free;

    char temp_buffer[BUFFER_SIZE + 1];

    std::shared_ptr<notifier> notifier_;

    bool terminating = false;
};

#endif //KIMBERLY_PROXY_SERVER_H