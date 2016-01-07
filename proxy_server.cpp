//
// Created by root on 07.01.16.
//

#include "proxy_server.h"
#include "util.h"

#include <sys/socket.h>
#include <unistd.h>

proxy_server::proxy_server(std::string host, int port) {
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

    CHK(listen(listenerSocket, 1));
    Log::d("Start to listen host");

    static struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listenerSocket;

    CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, listenerSocket, &ev));
    Log::d("Main listener added to epoll!");
}

void proxy_server::loop() {
    static struct epoll_event events[TARGET_CONNECTIONS];
    while (1) {
        int epoll_events_count;
        CHK2(epoll_events_count,
             epoll_wait(epfd, events, TARGET_CONNECTIONS, DEFAULT_TIMEOUT));
        Log::d("Epoll events count: " + inttostr(epoll_events_count));

        clock_t tStart = clock();

        for (int i = 0; i < epoll_events_count; i++) {
            Log::d("event " + inttostr(i) + ": " + eetostr(events[i]));
            // EPOLLIN event for listener(new client connection)
            if (events[i].data.fd == listenerSocket) {
                int client;
                struct sockaddr_in their_addr;
                socklen_t socklen = sizeof(struct sockaddr_in);
                CHK2(client,
                     accept(listenerSocket, (struct sockaddr *) &their_addr,
                            &socklen));

                Log::d("connection from:"
                       + std::string(inet_ntoa(their_addr.sin_addr))
                       + ":" + inttostr(ntohs(their_addr.sin_port))
                       + ", socket assigned to " + inttostr(client));
                // setup nonblocking socket
                setnonblocking(client);

                // set new client to event template
                static struct epoll_event ev;
                ev.data.fd = client;
                ev.events = EPOLLIN;

                // add new client to epoll
                CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, client, &ev));

                // save new descriptor to further use
                clients_list.push_back(client); // add new connection to list of clients
                Log::d("Added new client(fd = " + inttostr(client)
                       + ") to epoll and now clients_list.size = "
                       + inttostr(clients_list.size()));

                // send initial welcome message to client
                int res;
                std::string message = "Hello :)";
                CHK2(res, send(client, message.c_str(), message.length(), 0));
                Log::d("Message sended");
            } else { // EPOLLIN event for others(new incoming message from client)
                Log::d("Incoming message");
                int client = events[i].data.fd;
                char buf[1024];
                int len;
                CHK2(len, recv(client, buf, 1024, 0));
                if (len == 0) {
                    CHK(close(client));
                    clients_list.remove(client);
                    Log::d("Client with fd " + inttostr(client) + " has closed connection! And now we have "
                           + inttostr(clients_list.size()) + " clients");
                } else {
                    std::string message(buf);
                    message = message.substr(0, message.length() - 2);
                    Log::d("Client with fd " + inttostr(client) + " said '" + message + "'(len " +
                           inttostr(message.length()) + ")");

                    int res;
                    CHK2(res, send(client, message.c_str(), message.length(), 0));
                    CHK2(res, send(client, "\n", 1, 0));
                }
            }
            // print epoll events handling statistics
            printf("Statistics: %d events handled at: %.2f second(s)\n",
                   epoll_events_count,
                   (double) (clock() - tStart) / CLOCKS_PER_SEC);
            std::cout.flush();
        }
    }
}

void proxy_server::terminate() {
    close(listenerSocket);
    close(epfd);
}
