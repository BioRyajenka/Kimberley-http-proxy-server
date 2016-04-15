//
// Created by jackson on 14.04.16.
//

#ifndef KIMBERLY_HOSTNAME_RESOLVER_H
#define KIMBERLY_HOSTNAME_RESOLVER_H

#include <pthread.h>
#include "handler.h"

class hostname_resolver {
public:
    hostname_resolver(proxy_server *serv) : serv(serv), id(free_id++) { }

    void start();

private:
    pthread_t thread;
    proxy_server *serv;
    int id;

    static int free_id;
};

#endif //KIMBERLY_HOSTNAME_RESOLVER_H