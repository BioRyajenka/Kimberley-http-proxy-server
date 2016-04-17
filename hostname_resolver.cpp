//
// Created by jackson on 14.04.16.
//

#include <stddef.h>
#include <thread>
#include <stdlib.h>
#include "hostname_resolver.h"
#include "util.h"
#include "handler.h"

void hostname_resolver::start() {
    thread = new std::thread((std::function<void()>) ([this]() -> void {
        Log::d("resolver thread id: " + inttostr((int) pthread_self()));
        while (1) {
            //Log::d(std::string("serv is ") + (serv == nullptr ? "null" : "not null"));
            auto func = serv->hostname_resolve_queue.pop();
            //Log::d("VALUE IS " + inttostr(func));
            func();
            Log::d("RESOLVER: \tfunction successfully executed");

            serv->notify_epoll();
        }
    }));
}

int hostname_resolver::free_id = 0;