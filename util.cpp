//
// Created by root on 07.01.16.
//

#include <algorithm>
#include "util.h"

int strtoint(std::string s) {
    if (!s.length()) return 0;
    int res = 0;
    char is_negative = false;
    if (s[0] == '-') {
        is_negative = true;
        s = s.substr(1, s.length() - 1);
    }
    for (int i = 0; i < s.length(); i++) {
        res = res * 10 + s[i] - 48;
    }
    return res * (is_negative ? -1 : 1);
}

std::string inttostr(int n) {
    if (!n) return "0";
    std::string res = "";
    while (n) {
        res += char(n % 10 + 48);
        n /= 10;
    }
    std::reverse(res.begin(), res.end());
    return res;
}

std::string eetostr(epoll_event ev) {
    std::string result = "fd(" + inttostr(ev.data.fd) + "), ev.events:";
    if (ev.events & EPOLLIN)
        result += " EPOLLIN ";
    if (ev.events & EPOLLOUT)
        result += " EPOLLOUT ";
    if (ev.events & EPOLLET)
        result += " EPOLLET ";
    if (ev.events & EPOLLPRI)
        result += " EPOLLPRI ";
    if (ev.events & EPOLLRDNORM)
        result += " EPOLLRDNORM ";
    if (ev.events & EPOLLRDBAND)
        result += " EPOLLRDBAND ";
    if (ev.events & EPOLLWRNORM)
        result += " EPOLLRDNORM ";
    if (ev.events & EPOLLWRBAND)
        result += " EPOLLWRBAND ";
    if (ev.events & EPOLLMSG)
        result += " EPOLLMSG ";
    if (ev.events & EPOLLERR)
        result += " EPOLLERR ";
    if (ev.events & EPOLLHUP)
        result += " EPOLLHUP ";
    if (ev.events & EPOLLONESHOT)
        result += " EPOLLONESHOT ";

    return result;

}

int setnonblocking(int sockfd) {
    Log::d("Making " + inttostr(sockfd) + " non-blocking");
    CHK(fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK));
    return 0;
}

int Log::level = 0;
std::vector<std::ostream *> Log::targets;