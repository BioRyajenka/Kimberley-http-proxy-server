//
// Created by root on 08.01.16.
//

#include "handler.h"

bool server_handler::handle(epoll_event) {
    Log::d("Server handler");
    sockaddr_in client_addr;
    socklen_t ca_len = sizeof(client_addr);

    int client = accept(fd, (struct sockaddr *) &client_addr, &ca_len);

    if (client < 0) {
        Log::e("Error accepting connection");
        return false;
    }

    static struct epoll_event ev;
    ev.data.fd = client;
    ev.events = EPOLLIN;

    Log::d("Client connected: " + std::string(inet_ntoa(client_addr.sin_addr)) + ":" + inttostr(client_addr.sin_port));
    serv->add_handler(client, new echo_handler(client, serv), EPOLLIN);
    return true;
}

bool echo_handler::handle(epoll_event e) {
    Log::d("Echo handler: " + eetostr(e));
    if (e.events & EPOLLHUP) {
        serv->remove_handler(fd);
        return false;
    }

    if (e.events & EPOLLERR) {
        return false;
    }

    if (e.events & EPOLLOUT) {
        if (received > 0) {
            Log::d("Writing: " + std::string(buffer));
            if (send(fd, buffer, (size_t) received, 0) != received) {
                Log::e("Error writing to socket");
            }
        }

        serv->modify_handler(fd, EPOLLIN);
    }

    if (e.events & EPOLLIN) {
        if ((received = recv(fd, buffer, BUFFER_SIZE, 0)) < 0) {
            Log::e("Error reading from socket");
        } else if (received > 0) {
            buffer[received] = 0;
            Log::d("Reading: " + std::string(buffer));
        }

        if (received > 0) {
            serv->modify_handler(fd, EPOLLOUT);
        } else {
            serv->remove_handler(fd);
        }
    }

    return true;
}