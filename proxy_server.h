//
// Created by root on 07.01.16.
//

#ifndef KIMBERLY_PROXY_SERVER_H
#define KIMBERLY_PROXY_SERVER_H

#include <string>
#include <vector>
#include <arpa/inet.h>
#include <mutex>
#include <list>
#include "buffer.h"

class handler;

class client_handler;

class proxy_server {
public:
    proxy_server(std::string host, uint16_t port);

    void prepare();

    void loop();

    void terminate();

    void add_handler(int fd, handler *h, uint events);

    void modify_handler(int fd, uint events);

    void remove_handler(int fd);

    void queue_to_process(std::function<void()>);

#define BUFFER_SIZE 1024

    // returns true if large_buffer was totally sended to fd
    bool write_chunk(const handler &h, buffer &buf);

    // returns true if recv returned 0
    bool read_chunk(const handler &h, buffer &buf);

protected:
#define TARGET_CONNECTIONS 10000
    const int DEFAULT_TIMEOUT = -1;

    uint16_t port;
    in_addr_t host;
    int listenerSocket;

    int epfd;//main epoll

    std::vector<handler *> handlers;
    std::list<std::function<void()>> to_process;

    std::mutex to_process_mutex;

    char temp_buffer[BUFFER_SIZE];
};

#endif //KIMBERLY_PROXY_SERVER_H