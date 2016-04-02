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
        serv->remove_handler(fd);
        return false;
    }

    if (e.events & EPOLLERR) {
        return false;
    }

    if (e.events & EPOLLOUT) {
        assert(status == STATUS_WRITING_HOST_ANSWER);
        size_t to_send = (size_t) std::min((int) large_buffer.length() - bytes_sended, BUFFER_SIZE);
        if (send(fd, large_buffer.c_str() + bytes_sended, to_send, 0) != to_send) {
            Log::e("Error writing to socket");
        }
        bytes_sended += to_send;
        if (bytes_sended == large_buffer.length()) {
            serv->modify_handler(fd, EPOLLIN);
            status = STATUS_WAITING_FOR_MESSAGE;
        }
    }

    if (e.events & EPOLLIN) {
        assert(status == STATUS_WAITING_FOR_MESSAGE);
        ssize_t received;
        CHK2(received, recv(fd, buffer, BUFFER_SIZE, 0));
        if (received == 0) {
            serv->remove_handler(fd);
            return false;
        }
        buffer[received] = 0;
        Log::d("Status: " + inttostr(status) + ", received: \n\"" + std::string(buffer) + "\"");
        int plen = (int) large_buffer.length();
        large_buffer += buffer;
        if (find_double_line_break(large_buffer, plen) != -1) {
            assert(status == STATUS_WAITING_FOR_MESSAGE);

            serv->modify_handler(fd, 0);
            status = STATUS_WAITING_FOR_IP_RESOLVING;

            //TODO: new process
            resolve_host_ip(extract_property(large_buffer, (int) large_buffer.length(), "Host"));
        }
    }

    return true;
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
    }

    if (e.events & EPOLLERR) {
        Log::e("Very difficult situation2");
    }

    if (e.events & EPOLLOUT) {
        Log::d("client_request_handler:EPOLLOUT");
        assert(status == STATUS_WAITING_FOR_EPOLLOUT);

        size_t to_send = (size_t) std::min((int) clh->large_buffer.length() - clh->bytes_sended, BUFFER_SIZE);
        if (send(fd, clh->large_buffer.c_str() + clh->bytes_sended, to_send, 0) != to_send) {
            Log::e("Error writing to socket");
        }
        clh->bytes_sended += to_send;
        if (clh->bytes_sended == clh->large_buffer.length()) {
            Log::d("Finished resending query to host");
            status = STATUS_WAITING_FOR_ANSWER;
            serv->modify_handler(fd, EPOLLIN);
            std::string().swap(clh->large_buffer);//clearing
            response_len = -1;
        }
    }

    if (e.events & EPOLLIN) {
        Log::d("client_request_handler:EPOLLIN");
        assert(status == STATUS_WAITING_FOR_ANSWER);
        int plen = (int) clh->large_buffer.length();
        ssize_t received;
        CHK2(received, recv(fd, clh->buffer, BUFFER_SIZE, 0));
        if (received == 0) {
            //TODO: to do what??
            Log::e("Very difficult situation3");
            clh->large_buffer = "Very difficult situation3";
            serv->remove_handler(fd);
            serv->modify_handler(clh->fd, EPOLLOUT);
            clh->status = clh->STATUS_WRITING_HOST_ANSWER;
            clh->bytes_sended = 0;
        }
        clh->buffer[received] = 0;
        clh->large_buffer += clh->buffer;

        if (response_len == -1) {
            Log::d("plen is " + inttostr(plen));
            int lb = find_double_line_break(clh->large_buffer, plen);
            if (lb != -1) {
                Log::d("l_buffer is \n\"" + clh->large_buffer + "\"");
                Log::d("lb is \"" + inttostr(lb) + "\"");
                Log::d("fuck1 is \"" + inttostr((int)(clh->large_buffer[lb - 1])) + "\"");
                Log::d("fuck2 is \"" + inttostr((int)(clh->large_buffer[lb])) + "\"");
                std::string content_length_str = extract_property(clh->large_buffer, lb, "Content-Length");
                int len = 0;
                if (content_length_str != "") {
                    len = strtoint(content_length_str);
                }
                Log::d("len is " + inttostr(len));
                response_len = len + lb + 1;
            }
        }

        if (response_len != -1 && response_len == clh->large_buffer.length()) {
            serv->remove_handler(fd);
            serv->modify_handler(clh->fd, EPOLLOUT);
            clh->status = clh->STATUS_WRITING_HOST_ANSWER;
            clh->bytes_sended = 0;
        }
    }
}