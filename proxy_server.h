//
// Created by root on 07.01.16.
//

#ifndef KIMBERLY_PROXY_SERVER_H
#define KIMBERLY_PROXY_SERVER_H

#include <string>
#include <list>
#include <arpa/inet.h>

class proxy_server {
public:
    proxy_server(std::string host, int port);
    void prepare();
    void loop();
    void terminate();

protected:
#define TARGET_CONNECTIONS 10000
    const int DEFAULT_TIMEOUT = -1;

    uint16_t port;
    in_addr_t host;
    int listenerSocket;
    int epfd;//main epoll
    std::list<int> clients_list;
};

#endif //KIMBERLY_PROXY_SERVER_H
