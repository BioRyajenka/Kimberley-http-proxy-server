//
// Created by jackson on 14.04.16.
//

#ifndef KIMBERLY_HOSTNAME_RESOLVER_H
#define KIMBERLY_HOSTNAME_RESOLVER_H

#include <pthread.h>
#include <map>
#include <mutex>

class proxy_server;

class client_handler;

class hostname_resolver {
    friend class proxy_server;

public:
    hostname_resolver(proxy_server *serv) : serv(serv), id(free_id++) { }

    ~hostname_resolver();

    hostname_resolver(const hostname_resolver &) = delete;

    hostname_resolver &operator=(const hostname_resolver &) = delete;

    void start();

    static void add_task(proxy_server *serv, std::shared_ptr<client_handler> h, std::string hostname, uint flags);

private:
    proxy_server *serv;
    int id;

    static int free_id;

    std::thread *thread;

    static std::map<std::string, struct addrinfo *> cashed_hostnames;

    static std::mutex cashing_mutex;
};

#endif //KIMBERLY_HOSTNAME_RESOLVER_H