//
// Created by jackson on 14.04.16.
//

#ifndef KIMBERLY_HOSTNAME_RESOLVER_H
#define KIMBERLY_HOSTNAME_RESOLVER_H

#include <pthread.h>

class proxy_server;

class hostname_resolver {
public:
    hostname_resolver(proxy_server *serv) : serv(serv), id(free_id++) { }

    ~hostname_resolver();

    hostname_resolver(const hostname_resolver &) = delete;

    hostname_resolver &operator=(const hostname_resolver &) = delete;

    void start();

private:
    proxy_server *serv;
    int id;

    static int free_id;

    std::thread *thread;
};

#endif //KIMBERLY_HOSTNAME_RESOLVER_H