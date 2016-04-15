//
// Created by jackson on 14.04.16.
//

#include <stddef.h>
#include <thread>
#include <stdlib.h>
#include "hostname_resolver.h"
#include "util.h"

void hostname_resolver::start() {
    std::thread t((std::function<void()>)([this]() -> void {
        auto func = serv->hostname_resolve_queue.pop();
        func();

        serv->notify_epoll();
    }));
}

int hostname_resolver::free_id = 0;