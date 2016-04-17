//
// Created by jackson on 14.04.16.
//

#ifndef KIMBERLY_HOSTNAME_RESOLVER_H
#define KIMBERLY_HOSTNAME_RESOLVER_H

#include <pthread.h>
//#include "proxy_server.h"

class proxy_server;

class hostname_resolver {
public:
    hostname_resolver(proxy_server *serv) : serv(serv), id(free_id++) { }

    void start();

private:
    proxy_server *serv;
    int id;

    static int free_id;

    std::thread *thread;
};

#endif //KIMBERLY_HOSTNAME_RESOLVER_H