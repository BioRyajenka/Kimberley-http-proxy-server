//
// Created by root on 07.01.16.
//

#include <algorithm>
#include <assert.h>
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

std::string chartostr(char c) {
    return {c};
}

std::string inttostr(int n) {
    char is_negative = false;
    if (n < 0) {
        n *= -1;
        is_negative = true;
    }
    if (!n) return "0";
    std::string res = "";
    while (n) {
        res += char(n % 10 + 48);
        n /= 10;
    }
    res += (is_negative ? "-" : "");
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

bool is_break_char(char c) {
    return c == '\n' || c == '\r';
}

//since start till to
bool extract_property(std::string &s, int to, std::string name, std::string &result) {
    Log::d("Extracting property '" + name + "'");
    int s_pos = -1;
    int from = (int) name.length() + 1;
    for (int i = from; i < to; i++) {
        if (s[i - 1] != ':' || s[i] != ' ') continue;
        char ok = true;
        for (int j = 0; j < (int) name.length(); j++) {
            ok &= s[i - j - 2] == name[name.length() - j - 1];
        }
        if (ok) {
            s_pos = i + 1;
            break;
        }
    }

    if (s_pos == -1) {
        Log::d("...property was not found");
        return false; // not found
    }

    int f_pos = -1;
    for (int i = s_pos; i < to; i++) {
        if (is_break_char(s[i])) {
            f_pos = i;
            break;
        }
    }
    assert(f_pos != -1);
    result = s.substr((unsigned long long) s_pos, (unsigned long long) f_pos - s_pos);
    Log::d("...value is " + result);
    return true;
}

/*13 10
13 10
 or
10
13 10*/

//since from - 1 to the end
int find_double_line_break(std::string &s, int from) {
    for (int i = std::max(1, from - 1); i < s.length(); i++)
        if (s[i - 1] == 10 && is_break_char(s[i])) {
            while (i < s.length() && is_break_char(s[i++]));
            return i - 1;
        }
    return -1;
}