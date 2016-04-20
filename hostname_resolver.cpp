//
// Created by jackson on 14.04.16.
//

#include <stddef.h>
#include <thread>
#include <stdlib.h>
#include "hostname_resolver.h"
#include "util.h"
#include "handler.h"

hostname_resolver::~hostname_resolver() {
    if (!serv->hostname_resolve_queue.is_releasing()) {
        Log::fatal("hostname_resolve_queue should be released first");
    }
    thread->join();
    delete thread;
}

void hostname_resolver::start() {
    thread = new std::thread((std::function<void()>) ([this] {
        Log::d("resolver thread id: " + inttostr((int) pthread_self()));
        while (1) {
            //Log::d(std::string("serv is ") + (serv == nullptr ? "null" : "not null"));
            std::function<void()> func;
            if (!serv->hostname_resolve_queue.pop(func)) {
                return;
            }
            //Log::d("VALUE IS " + inttostr(func));
            func();
            Log::d("RESOLVER: \tfunction successfully executed");

            serv->notify_epoll();
        }
    }));

}

int hostname_resolver::free_id = 0;