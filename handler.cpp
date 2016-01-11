//
// Created by root on 08.01.16.
//

#include "handler.h"
#include <cassert>
#include <netdb.h>
#include <string.h>

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
        /*if (received > 0) {
            Log::d("Writing: " + std::string(buffer2));
            if (send(fd, buffer2, (size_t) received, 0) != received) {
                Log::e("Error writing to socket");
            }
        }*/

        serv->modify_handler(fd, EPOLLIN);
    }

    if (e.events & EPOLLIN) {
        if ((received = recv(fd, temp_buffer, BUFFER_SIZE, 0)) < 0) {
            Log::e("Error reading from socket");
        } else if (received > 0) {
            temp_buffer[received] = 0;
            //Log::d("Received \"" + std::string(temp_buffer) + "\"");

            size_t prev_size = buffer.length();
            buffer.append(temp_buffer);
            Log::d("Now buffer is \"" + buffer + "\"");
            Log::d(inttostr(host_token_pos));
            if (host_token_pos == -1) {
                for (size_t i = std::max(prev_size, (size_t)6); i < buffer.length(); i++) {
                    if (buffer[i - 1] == ':' && buffer[i - 2] == 't' && buffer[i - 3] == 's' && buffer[i - 4] == 'o' &&
                        buffer[i - 5] == 'H' && buffer[i - 6] == '\n' && buffer[i] == ' ') {
                        host_token_pos = (int)(i + 1);
                        Log::d("found host_token_pos " + inttostr(host_token_pos));
                        break;
                    }
                }
            }
            if (host_token_pos != -1) {
                Log::d("status " + inttostr(status));
                if (status != STATUS_WAITING_FOR_URL_RESOLVING) {
                    assert(host_token_pos != -1);
                    int line_break = -1;
                    for (size_t i = (size_t)host_token_pos; i < buffer.length(); i++) {
                        if (buffer[i] == '\n' || buffer[i] == '\r') {
                            line_break = (int) i;
                            break;
                        }
                    }
                    Log::d("found line break in " + inttostr(line_break));
                    if (line_break != -1) {
                        //TODO: in separate thread
                        char pchar = buffer[line_break];
                        buffer[line_break] = 0;

                        strcpy(temp_buffer, buffer.c_str() + host_token_pos);
                        Log::d("url is \"" + std::string(temp_buffer) + "\"");

                        buffer[line_break] = pchar;

                        struct hostent *he;//TODO: to take out
                        he = gethostbyname(temp_buffer);
                        for(int i = 0; (struct in_addr **)he->h_addr_list[i] != NULL; i++) {
                            Log::d(std::string(inet_ntoa(*((struct in_addr *) he->h_addr_list[i]))));
                        }
                    }
                }
            }
        }

        //TODO: errno & EAGAIN
        if (received > 0) {
            //serv->modify_handler(fd, EPOLLOUT);
        } else {
            serv->remove_handler(fd);
        }
    }

    return true;
}