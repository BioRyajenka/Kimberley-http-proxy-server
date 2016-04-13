//
// Created by root on 08.01.16.
//

#include "handler.h"
#include <cassert>
#include <netdb.h>

void server_handler::handle(const epoll_event &) {
    Log::d("Server handler");
    sockaddr_in client_addr;
    socklen_t ca_len = sizeof(client_addr);

    int client = accept(fd, (struct sockaddr *) &client_addr, &ca_len);

    if (client < 0) {
        Log::fatal("Error accepting connection");
        //return false;
    }

    static struct epoll_event ev;
    ev.data.fd = client;
    ev.events = EPOLLIN;

    Log::d("Client connected: " + std::string(inet_ntoa(client_addr.sin_addr)) + ":" + inttostr(client_addr.sin_port));
    serv->add_handler(client, new client_handler(client, serv), EPOLLIN);
}

bool client_handler::read_message(const handler &h, buffer &buf) {
    int plen = buf.length();

    if (serv->read_chunk(h, buf)) {
        Log::d("fd(" + inttostr(h.fd) + ") asked for disconnection");
        disconnect();
        return false;
    }
    if (message_type == NOT_EVALUATED) {
        // recalc message len
        int lb = find_double_line_break(buf.string_data(), plen);
        if (lb != -1) {
            //Log::d("plen is " + inttostr(plen) + ", lb is " + inttostr(lb) + " in \n\"" + buf.string_data() + "\"");

            std::string content_length_str;
            int len;
            if (extract_header(buf.string_data(), lb, "Content-Length", content_length_str)) {
                len = strtoint(content_length_str);
                message_type = VIA_CONTENT_LENGTH;
            } else {
                len = 0;
                message_type = extract_header(buf.string_data(), lb, "Transfer-Encoding", content_length_str)
                               ? VIA_TRANSFER_ENCODING : WITHOUT_BODY;
            }
            message_len = len + lb;
        }
    }

    if (message_type != HTTPS_MODE) {
        Log::d("read_message(" + inttostr(buf.length()) + ", " + inttostr(message_len) + ")");
    }

    if (message_type == NOT_EVALUATED) {
        return false;
    }

    if (message_type == VIA_TRANSFER_ENCODING) {
        while (buf.length() > message_len) {
            Log::d("kek is  " + inttostr((int) buf.string_data()[message_len - 1]) + " " +
                   inttostr((int) buf.string_data()[message_len]) + " " +
                   inttostr((int) buf.string_data()[message_len + 1]));
            if (buf.string_data()[message_len] == '0') {
                return true;
            }
            size_t linebreak = buf.string_data().find("\r\n", (size_t) message_len);
            if (linebreak != std::string::npos) {
                std::string chunklen = buf.string_data().substr((size_t) message_len, linebreak - message_len);
                Log::d("Next chunk len is " + chunklen);
                message_len += chunklen.length() + hextoint(chunklen) + 4;
            }
        }
        return false;
    } else {
        return buf.length() == message_len;
    }
}

void client_handler::handle(const epoll_event &e) {
    if (!(e.events & EPOLLOUT) || (e.events & EPOLLIN) || !output_buffer.empty()) {
        Log::d("Client handler: " + eetostr(e));
    }
    if (e.events & EPOLLOUT) {
        if (serv->write_chunk(*this, output_buffer) && message_type != HTTPS_MODE) {
            Log::d("Finished resending host response to client");
            if (message_type == PRE_HTTPS_MODE) {
                message_type = HTTPS_MODE;

                std::string hostname;
                extract_header(input_buffer.string_data(), input_buffer.length(), "Host", hostname);
                input_buffer.clear();
                output_buffer.clear();
                serv->modify_handler(fd, EPOLLIN | EPOLLOUT);
                resolve_host_ip(hostname, EPOLLIN | EPOLLOUT);
            } else {
                input_buffer.clear();
                message_len = -1;
                message_type = NOT_EVALUATED;
                serv->modify_handler(fd, EPOLLIN);
            }
        }
    }

    if (e.events & EPOLLIN) {
        if (read_message(*this, input_buffer) && message_type != HTTPS_MODE) {
            std::string data = input_buffer.string_data();

            serv->modify_handler(fd, 0);

            std::string hostname;
            if (extract_header(data, (int) data.length(), "Host", hostname)) {
                if (message_type == WITHOUT_BODY && extract_method(data) == "CONNECT") {
                    //output_buffer.set("HTTP/1.0 404 Fail\r\nProxy-agent: BotHQ-Agent/1.2\r\n\r\n");
                    output_buffer.set("HTTP/1.0 200 OK\r\nProxy-agent: BotHQ-Agent/1.2\r\n\r\n");
                    serv->modify_handler(fd, EPOLLOUT);
                    message_type = PRE_HTTPS_MODE;
                    Log::d("entering HTTPS_MODE");
                } else {
                    if (message_type == WITHOUT_BODY && extract_method(data) == "GET") {
                        // modifying status line
                        size_t from = data.find(hostname) + hostname.length();

                        data = "GET " +
                               input_buffer.string_data().substr(from, input_buffer.string_data().length() - from);
                        input_buffer.clear();
                        input_buffer.put(data.c_str(), data.length());

                        Log::d("new query is: \"\n" + input_buffer.string_data() + "\"");
                    }

                    resolve_host_ip(hostname, EPOLLOUT);
                }
            } else {
                //TODO: send correspond answer
                Log::fatal("Client headers do not contain 'host' header");
                //exit(-1);
            }
        }
    }
}

void client_handler::resolve_host_ip(std::string hostname, const uint &flags) {
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

    if ((he = gethostbyname(hostname.c_str())) == 0) {
        Log::d("hostname " + hostname + " cannot be resolved");
        output_buffer.set("HTTP/1.1 404 Not Found\r\nContent-Length: 26\r\n\r\n<html>404 not found</html>");
        serv->modify_handler(fd, EPOLLOUT);
        return;
    }

    for (int i = 0; (struct in_addr **) he->h_addr_list[i] != NULL; i++) {
        Log::d("ips[" + inttostr(i) + "] = " + std::string(inet_ntoa(*((struct in_addr *) he->h_addr_list[i]))));
    }

    Log::d("Ip is " + std::string(inet_ntoa(*((struct in_addr *) he->h_addr_list[0]))));

    CHK2(_client_request_socket, socket(AF_INET, SOCK_STREAM, 0));

    setnonblocking(_client_request_socket);

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *) he->h_addr_list[0])));
    addr.sin_port = htons(port);

    connect(_client_request_socket, (struct sockaddr *) &addr, sizeof(addr));
    if (errno != EINPROGRESS) {
        perror("connect");
        Log::fatal("connect");
    }

    serv->queue_to_process([this, flags]() {
        Log::d("Creating client_request socket fd(" + inttostr(_client_request_socket) + ") for client fd(" +
               inttostr(fd) +
               ")");
        serv->add_handler(_client_request_socket,
                          new client_handler::client_request_handler(_client_request_socket, serv, this), flags);
    });
}

void client_handler::client_request_handler::handle(const epoll_event &e) {
    if (!(e.events & EPOLLOUT) || (e.events & EPOLLIN) || !clh->input_buffer.empty()) {
        Log::d("Client request handler: " + eetostr(e));
    }
    if (e.events & EPOLLOUT) {
        if (serv->write_chunk(*this, clh->input_buffer) && clh->message_type != HTTPS_MODE) {
            Log::d("Finished resending query to host");
            serv->modify_handler(fd, EPOLLIN);
            clh->output_buffer.clear();

            clh->message_len = -1;
            clh->message_type = NOT_EVALUATED;
        }
    }

    if (e.events & EPOLLIN) {
        if (clh->read_message(*this, clh->output_buffer) && clh->message_type != HTTPS_MODE) {
            Log::d("It seems that all message was received.");
            disconnect(); // only me
            serv->modify_handler(clh->fd, EPOLLOUT);
        }
    }
}