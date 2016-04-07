//
// Created by root on 08.01.16.
//

#include "handler.h"
#include <cassert>
#include <netdb.h>

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
    serv->add_handler(client, new client_handler(client, serv), EPOLLIN);
    return true;
}

bool client_handler::handle(epoll_event e) {
    Log::d("Client handler: " + eetostr(e));
    if (e.events & EPOLLHUP) {
        Log::e("EPOLLHUP");
        serv->remove_handler(fd);
        return false;
    }

    if (e.events & EPOLLERR) {
        Log::e("EPOLLERR");
        return false;
    }

    if (e.events & EPOLLOUT) {
        assert(status == STATUS_WRITING_HOST_ANSWER);
        size_t to_send = std::min(large_buffer.length() - bytes_sended, (size_t) BUFFER_SIZE);
        if (send(fd, large_buffer.c_str() + bytes_sended, to_send, 0) != to_send) {
            Log::e("Error writing to socket");
        }
        bytes_sended += to_send;
        if (bytes_sended == large_buffer.length()) {
            Log::d("Finished resending host response to clients");
            std::string().swap(large_buffer);//clearing
            serv->modify_handler(fd, EPOLLIN);
            status = STATUS_WAITING_FOR_MESSAGE;
        }
        return true;
    }

    if (e.events & EPOLLIN) {
        assert(status == STATUS_WAITING_FOR_MESSAGE);
        ssize_t received;
        CHK2(received, recv(fd, buffer, BUFFER_SIZE, 0));
        if (received == 0) {
            serv->remove_handler(fd);
            return false;
        }
        Log::d("Status: " + inttostr(status) + ", received: \n\"" + std::string(buffer) + "\"");
        int plen = (int) large_buffer.length();
        large_buffer += std::string(buffer, received);
        if (find_double_line_break(large_buffer, plen) != -1) {
            assert(status == STATUS_WAITING_FOR_MESSAGE);

            serv->modify_handler(fd, 0);
            status = STATUS_WAITING_FOR_IP_RESOLVING;

            //TODO: new process
            resolve_host_ip(extract_property(large_buffer, (int) large_buffer.length(), "Host"));
        }
        return true;
    }

    return false;
}

void client_handler::resolve_host_ip(std::string hostname) {
    Log::d("Resolving hostname \"" + hostname + "\"");
    uint16_t port = 80;

    for (size_t i = 0; i < hostname.length(); i++) {
        if (hostname[i] == ':') {
            port = (uint16_t) strtoint(hostname.substr(i + 1));
            hostname = hostname.substr(0, i);
            break;
        }
    }

    Log::d("hostname is " + hostname + ", port is " + inttostr(port));

//    if (hostname != "www.kgeorgiy.info") {
//        return;
//    }

    struct hostent *he;
    he = gethostbyname(hostname.c_str());

    //TODO: do smth in case of absence of network
    //for (int i = 0; (struct in_addr **) he->h_addr_list[i] != NULL; i++) {
    //    Log::d(std::string(inet_ntoa(*((struct in_addr *) he->h_addr_list[i]))));
    //}

    Log::d("Ip is " + std::string(inet_ntoa(*((struct in_addr *) he->h_addr_list[0]))));

    CHK2(_client_request_socket, socket(PF_INET, SOCK_STREAM, 0));
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *) he->h_addr_list[0])));
    addr.sin_port = htons(port);

    CHK(connect(_client_request_socket, (struct sockaddr *) &addr, sizeof(addr)));

    serv->to_process_mutex.lock();
    serv->queue_to_process(this);
    serv->to_process_mutex.unlock();

    status = STATUS_WAITING_FOR_CREATING_REQUEST_HANDLER;
}

void client_handler::process() {
    Log::d("process()");
    bytes_sended = 0;
    status = STATUS_WAITING_FOR_HOST_ANSWER;
    serv->add_handler(_client_request_socket,
                      new client_handler::client_request_handler(_client_request_socket, serv, this), EPOLLOUT);
}

bool client_handler::client_request_handler::handle(epoll_event e) {
    if (e.events & EPOLLHUP) {
        Log::e("Very difficult situation1");
        serv->remove_handler(fd);
        serv->remove_handler(clh->fd);
        return false;
    }

    if (e.events & EPOLLERR) {
        Log::e("Very difficult situation2");
        serv->remove_handler(fd);
        serv->remove_handler(clh->fd);
        return false;
    }

    if (e.events & EPOLLOUT) {
        Log::d("client_request_handler:EPOLLOUT");
        assert(status == STATUS_WAITING_FOR_EPOLLOUT);

        size_t to_send = std::min(clh->large_buffer.length() - clh->bytes_sended, (size_t) BUFFER_SIZE);
        if (send(fd, clh->large_buffer.c_str() + clh->bytes_sended, to_send, 0) != to_send) {
            Log::e("Error writing to socket");
            serv->remove_handler(fd);
            serv->remove_handler(clh->fd);
            return false;
        }
        clh->bytes_sended += to_send;
        if (clh->bytes_sended == clh->large_buffer.length()) {
            Log::d("Finished resending query to host");
            status = STATUS_WAITING_FOR_ANSWER;
            serv->modify_handler(fd, EPOLLIN);

            std::string().swap(clh->large_buffer);//clearing
            response_len = -1;
        }
        return true;
    }

    if (e.events & EPOLLIN) {
        Log::d("client_request_handler:EPOLLIN");
        assert(status == STATUS_WAITING_FOR_ANSWER);
        ssize_t received;
        if ((received = recv(fd, clh->buffer, BUFFER_SIZE, 0)) == 0) {
            Log::e("Very difficult situation3: \n\"" + clh->large_buffer + "\"");
            assert(false);
        }
        int iteration = 0;
        do {
            if (received < 0) {
                perror("recv in client_request_handler");
                assert(false);
            }
            int plen = (int) clh->large_buffer.length();

            clh->large_buffer += std::string(clh->buffer, received);
            Log::d("received (" + inttostr(received) + ", " + inttostr(iteration++) + ", " + inttostr(response_len) +
                   ", " + inttostr(clh->large_buffer.length()) + "): \n\"" + clh->buffer + "\"");

            if (response_len == -1) {
                int lb = find_double_line_break(clh->large_buffer, plen);
                if (lb != -1) {
                    std::string content_length_str;
                    bool property_exists = extract_property(clh->large_buffer, lb, "Content-Length", content_length_str);
                    int len;
                    if (property_exists) {
                        len = strtoint(content_length_str);
                    } else {
                        property_exists = extract_property(clh->large_buffer, lb, "Transfer-Encoding", content_length_str);
                        if (property_exists) {
                            if (content_length_str == "chunked") {
                                
                            } else {
                                // other encoding methods are don't taken into account
                                len = 1;
                            }
                        } else {
                            len = 1;
                        }
                    }
                    response_len = len + lb;
                    Log::d("New response len is " + inttostr(response_len));
                }
            }

            if (response_len != -1 && response_len == clh->large_buffer.length()) {
                Log::d("It seems that all message received.");
                serv->remove_handler(fd);
                serv->modify_handler(clh->fd, EPOLLOUT);
                clh->status = clh->STATUS_WRITING_HOST_ANSWER;
                clh->bytes_sended = 0;
                return true;
            }
        } while (received = recv(fd, clh->buffer, BUFFER_SIZE, 0));
        return true;
    }
    return false;
}