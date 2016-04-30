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
#include "file_descriptor.h"

class handler;

class client_handler;

class notifier;

#define BUFFER_SIZE 1024
#define TARGET_CONNECTIONS 1000
#define RESOLVER_THREADS 100
#define CONNECTION_TIMEOUT 3

//template <int TARGET_CONNECTIONS = 1000, int RESOLVER_THREADS = 20, int CONNECTION_TIMEOUT = 1, int BUFFER_SIZE = 1024>
class proxy_server {
    friend class handler;

    friend class notifier;

    friend class client_handler;

    friend class server_handler;

    friend class hostname_resolver;

public:
    proxy_server(std::string host, uint16_t port, int resolver_threads = RESOLVER_THREADS);

    void loop();

    void terminate();

protected:
    void add_handler(std::shared_ptr<handler> h, const uint &events);

    void modify_handler(const handler *h, uint events);

    void remove_handler(const handler *h);

    // returns true if large_buffer was totally sended to fd
    bool write_chunk(handler *h, buffer &buf);

    // returns true if recv returned 0
    bool read_chunk(handler *h, buffer *buf);

    void notify_epoll();

    void add_resolver_task(int fd, std::string hostname, uint flags);

private:
    uint16_t port;
    in_addr_t host;

    file_descriptor epfd;//main epoll

    std::vector<std::shared_ptr<handler>> handlers;

    concurrent_queue<std::function<void()>> to_run;

    char temp_buffer[BUFFER_SIZE + 1];

    std::shared_ptr<notifier> notifier_;

    bool terminating = false;

    hostname_resolver resolver;

    //hostname_resolver resolver;
};

#endif //KIMBERLY_PROXY_SERVER_H