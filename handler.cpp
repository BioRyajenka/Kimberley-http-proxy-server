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
        Log::fatal("Error accepting connection");
        //return false;
    }

    static struct epoll_event ev;
    ev.data.fd = client;
    ev.events = EPOLLIN;

    Log::d("Client connected: " + std::string(inet_ntoa(client_addr.sin_addr)) + ":" + inttostr(client_addr.sin_port));
    serv->add_handler(client, new client_handler(client, serv), EPOLLIN);
    return true;
}

bool client_handler::write_chunk(const handler &h) {
    size_t to_send = std::min(large_buffer.length() - bytes_sended, (size_t) BUFFER_SIZE);
    if (send(h.fd, large_buffer.c_str() + bytes_sended, to_send, 0) != to_send) {
        Log::fatal("Error writing to socket");
        //exit(-1);
    }
    bytes_sended += to_send;
    return bytes_sended == large_buffer.length();
}

bool client_handler::read_chunk(const handler &h) {
    ssize_t received;
    if ((received = recv(h.fd, buffer, BUFFER_SIZE, 0)) < 0) {
        perror("read_chunk");
        Log::fatal("fatal in read_chunk");
        //exit(-1);
    }
    large_buffer += std::string(buffer, received);
    buffer[received] = 0;
    return received == 0;
}

bool client_handler::read_message(const handler &h) {
    int plen = (int) large_buffer.length();

    if (read_chunk(h)) {
        Log::w("Very difficult situation. Disconnecting");
        h.disconnect();
        return false;
    }
    if (message_type == NOT_EVALUATED) {
        // recalc message len
        int lb = find_double_line_break(large_buffer, plen);
        if (lb != -1) {
            std::string content_length_str;
            int len;
            if (extract_property(large_buffer, lb, "Content-Length", content_length_str)) {
                len = strtoint(content_length_str);
                message_type = VIA_CONTENT_LENGTH;
            } else {
                len = 0;
                message_type = extract_property(large_buffer, lb, "Transfer-Encoding", content_length_str)
                               ? VIA_TRANSFER_ENCODING : WITHOUT_BODY;
            }
            message_len = len + lb;
        }
    }

    Log::d("read_message (" + inttostr(large_buffer.length()) + ", " + inttostr(message_len) + "): \n\"" +
           buffer + "\"");

    if (message_type == NOT_EVALUATED) {
        return false;
    }
    if (message_type == VIA_TRANSFER_ENCODING) {
        if (large_buffer.length() > message_len) {
            Log::d("kek is  " + inttostr((int) large_buffer[message_len - 1]) + " " +
                   inttostr((int) large_buffer[message_len]) + " " + inttostr((int) large_buffer[message_len + 1]));
            if (large_buffer[message_len] == '0') {
                return true;
            }
            size_t linebreak = large_buffer.find("\r\n", message_len);
            if (linebreak != std::string::npos) {
                std::string chunklen = large_buffer.substr(message_len, linebreak - message_len);
                Log::d("Next chunk len is " + chunklen);
                message_len = message_len + chunklen.length() + hextoint(chunklen) + 4;
            }
            return false;
        }
    } else {
        return large_buffer.length() == message_len;
    }
}

bool client_handler::handle(epoll_event e) {
    Log::d("Client handler: " + eetostr(e));
    if (e.events & EPOLLOUT) {
        if (write_chunk(*this)) {
            Log::d("Finished resending host response to client");
            std::string().swap(large_buffer);//clearing
            message_len = -1;
            message_type = NOT_EVALUATED;
            serv->modify_handler(fd, EPOLLIN);
        }
        return true;
    }

    if (e.events & EPOLLIN) {
        if (read_message(*this)) {
            if (message_type == WITHOUT_BODY && large_buffer.substr(0, large_buffer.find(' ')) == "CONNECT") {
                large_buffer = "HTTP/1.0 200 Connection established\r\nProxy-agent: BotHQ-Agent/1.2\r\n\r\n";
                bytes_sended = 0;
                serv->modify_handler(fd, EPOLLOUT);
                return true;
            }

            serv->modify_handler(fd, 0);

            //TODO: new process
            std::string hostname;
            if (extract_property(large_buffer, (int) large_buffer.length(), "Host", hostname)) {
                resolve_host_ip(hostname);
            } else {
                Log::fatal("Client headers do not contain 'host' header");
                //exit(-1);
            }
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

    struct hostent *he;

    if ((he = gethostbyname(hostname.c_str())) == 0) {
        Log::d("hostname " + hostname + " cannot be resolved");
        large_buffer = "HTTP/1.1 404 Not Found\r\nContent-Length: 26\r\n\r\n<html>404 not found</html>";
        bytes_sended = 0;
        serv->modify_handler(fd, EPOLLOUT);
        return;
    }

    /*for (int i = 0; (struct in_addr **) he->h_addr_list[i] != NULL; i++) {
        Log::d("ips[i] = " + inttostr(i) + ", " + std::string(inet_ntoa(*((struct in_addr *) he->h_addr_list[i]))));
    }*/

    Log::d("Ip is " + std::string(inet_ntoa(*((struct in_addr *) he->h_addr_list[0]))));

    CHK2(_client_request_socket, socket(PF_INET, SOCK_STREAM, 0));

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *) he->h_addr_list[0])));
    addr.sin_port = htons(port);

    CHK(connect(_client_request_socket, (struct sockaddr *) &addr, sizeof(addr)));

    setnonblocking(_client_request_socket);

    serv->to_process_mutex.lock();
    serv->queue_to_process(this);
    serv->to_process_mutex.unlock();
}

void client_handler::process() {
    Log::d("process()");
    bytes_sended = 0;
    Log::d("Creating client_request socket fd(" + inttostr(_client_request_socket) + ") for client fd(" + inttostr(fd) +
           ")");
    serv->add_handler(_client_request_socket,
                      new client_handler::client_request_handler(_client_request_socket, serv, this), EPOLLOUT);
}

bool client_handler::client_request_handler::handle(epoll_event e) {
    Log::d("Client request handler: " + eetostr(e));
    if (e.events & EPOLLOUT) {
        if (clh->write_chunk(*this)) {
            Log::d("Finished resending query to host");
            serv->modify_handler(fd, EPOLLIN);

            std::string().swap(clh->large_buffer);//clearing
            clh->message_len = -1;
            clh->message_type = NOT_EVALUATED;
        }
        return true;
    }

    if (e.events & EPOLLIN) {
        if (clh->read_message(*this)) {
            Log::d("It seems that all message was received.");
            serv->remove_handler(fd);
            serv->modify_handler(clh->fd, EPOLLOUT);
            clh->bytes_sended = 0;
            return true;
        }
        return true;
    }
    return false;
}