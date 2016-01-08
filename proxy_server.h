//
// Created by root on 07.01.16.
//

#ifndef KIMBERLY_PROXY_SERVER_H
#define KIMBERLY_PROXY_SERVER_H

#include <string>
#include <vector>
#include <arpa/inet.h>

class handler;

class proxy_server {
public:
    proxy_server(std::string host, uint16_t port);

    void prepare();

    void loop();

    void terminate();

    void add_handler(int fd, handler *h, uint events);

    void modify_handler(int fd, uint events);

    void remove_handler(int fd);

    int epfd;//main epoll

protected:
#define TARGET_CONNECTIONS 10000
    const int DEFAULT_TIMEOUT = -1;

    uint16_t port;
    in_addr_t host;
    int listenerSocket;

    std::vector<handler *> handlers;
};

#endif //KIMBERLY_PROXY_SERVER_H
