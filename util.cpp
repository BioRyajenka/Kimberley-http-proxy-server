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

std::string chartostr(const char &c) {
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

int hextoint(const std::string &s) {
    Log::d("hex " + s + " to string " + inttostr((int) std::stoul("0x" + s, nullptr, 16)));
    return (int) std::stoul("0x" + s, nullptr, 16);
}

std::string eeflagstostr(const int &flags) {
    std::string result = "";
    if (flags & EPOLLIN)
        result += " EPOLLIN ";
    if (flags & EPOLLOUT)
        result += " EPOLLOUT ";
    if (flags & EPOLLERR)
        result += " EPOLLERR ";
    if (flags & EPOLLHUP)
        result += " EPOLLHUP ";
    return result;
}

std::string eetostr(const epoll_event &ev) {
    return "fd(" + inttostr(ev.data.fd) + "), ev.events:" + eeflagstostr(ev.events);
}

int setnonblocking(const int &sockfd) {
    Log::d("Making fd(" + inttostr(sockfd) + ") non-blocking");
    if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK) < 0) {
        perror("fcntl");
        Log::fatal("fatal");
    }
    return 0;
}

int Log::level = 0;
std::vector<std::ostream *> Log::targets;
std::mutex Log::mutex;

bool is_break_char(char c) {
    return c == '\n' || c == '\r';
}

//since start till to
bool extract_header(const std::string &s, int to, std::string name, std::string &result) {
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

    int f_pos = (int) s.find("\r\n", (ulong) s_pos);

    result = s.substr((ulong) s_pos, (ulong) f_pos - s_pos);
    Log::d("...value is " + result);
    return true;
}

/*13 10
13 10
 or
10
13 10*/

//returns position next to \r\n. (e.g. 2 for string \r\na and 3 for string a\r\n)
int find_double_line_break(const std::string &s, int from) {
    ulong res = s.find("\r\n\r\n", (ulong) from);
    return (res == std::string::npos ? -1 : (int) res + 4);
}

std::string extract_method(const std::string &s) {
    return s.substr(0, s.find(' '));
}

void set_status_line(std::string &s, const std::string &status_line) {
    size_t linebreak = s.find("\r\n");
    s = status_line + s.substr(linebreak, s.length() - linebreak);
}