//
// Created by root on 07.01.16.
//

#include "proxy_server.h"
#include "util.h"

#include <sys/socket.h>
#include <unistd.h>
#include "handler.h"

proxy_server::proxy_server(std::string host, uint16_t port) {
    proxy_server::port = htons(port);
    proxy_server::host = inet_addr(host.c_str());

    if (proxy_server::host == uint32_t(-1)) {
        Log::e("Cannot assign requested address: " + host);
        exit(-1);
    }

    struct in_addr antelope;
    antelope.s_addr = proxy_server::host;
    Log::d("Server host is " + std::string(inet_ntoa(antelope)) + ", port is "
           + inttostr(ntohs(proxy_server::port)));

    CHK2(listenerSocket, socket(PF_INET, SOCK_STREAM, 0));
    Log::d("Main listener(fd=" + inttostr(listenerSocket) + ") created!");
    setnonblocking(listenerSocket);

    CHK2(epfd, epoll_create(TARGET_CONNECTIONS));
    Log::d("Epoll(fd=" + inttostr(epfd) + ") created!");
}

void proxy_server::prepare() {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = host;
    addr.sin_port = port;

    CHK(bind(listenerSocket, (struct sockaddr *) &addr, sizeof(addr)));
    Log::d("Listener binded to host");

    CHK(listen(listenerSocket, 10));
    Log::d("Start to listen host");

    add_handler(listenerSocket, new server_handler(listenerSocket, this), EPOLLIN);
}

void proxy_server::loop() {
    Log::d("Start looping");
    static struct epoll_event events[TARGET_CONNECTIONS];
    while (1) {
        int epoll_events_count;
        CHK2(epoll_events_count, epoll_wait(epfd, events, TARGET_CONNECTIONS, DEFAULT_TIMEOUT));
        Log::d("Epoll events count: " + inttostr(epoll_events_count));

        clock_t tStart = clock();

        for (int i = 0; i < epoll_events_count; i++) {
            Log::d("event " + inttostr(i) + ": " + eetostr(events[i]));
            handlers[events[i].data.fd]->handle(events[i]);
        }

        printf("Statistics: %d events handled at: %.2f second(s)\n", epoll_events_count,
               (double) (clock() - tStart) / CLOCKS_PER_SEC);
        std::cout.flush();
    }
}


void proxy_server::terminate() {
    //TODO: terminate all clients
    close(listenerSocket);
    close(epfd);
}

void proxy_server::add_handler(int fd, handler *h, uint events) {
    Log::d("Trying to insert handler to fd " + inttostr(fd));
    if (handlers.size() <= fd) {
        handlers.resize(fd + 1);
    }
    handlers[fd] = h;
    Log::d("And now handlers[" + inttostr(fd) + "] = " + (handlers[fd] ? "yes" : "no") + ", handlers.size() " + inttostr(handlers.size()));
    epoll_event e;
    e.data.fd = fd;
    e.events = events;

    if (epoll_ctl(this->epfd, EPOLL_CTL_ADD, fd, &e) < 0) {
        Log::e("Failed to insert handler to epoll");
        perror(("add_handler (fd=" + inttostr(fd) + ")").c_str());
        std::cerr.flush();
    } else {
        Log::d("success");
    }
}

void proxy_server::modify_handler(int fd, uint events) {
    struct epoll_event e;

    if (fd >= handlers.size() || !handlers[fd]) {
        Log::e("Changing handler of unregistered file descriptor: " + inttostr(fd));
        Log::d("size is " + inttostr(handlers.size()) + ", handlers[fd] is " + (handlers[fd] ? "yes" : "no"));
        return;
    }

    e.data.fd = fd;
    e.events = events;

    if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &e) < 0) {
        Log::e("Failed to modify epoll events " + eetostr(e));
    }
}

void proxy_server::remove_handler(int fd) {
    if (fd >= handlers.size() || !handlers[fd]) {
        Log::e("Removing handler of a non registered file descriptor");
        return;
    }

    handlers[fd] = 0;

    struct epoll_event e;
    e.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &e) < 0) {
        Log::d("Failed to modify epoll events");
    }
}