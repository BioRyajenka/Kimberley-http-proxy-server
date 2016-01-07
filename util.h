//
// Created by root on 07.01.16.
//

#ifndef KIMBERLY_UTIL_H
#define KIMBERLY_UTIL_H

#include <iostream>
#include <fcntl.h>
#include <vector>
#include <sys/epoll.h>

// Macros - exit in any error (eval < 0) case
#define CHK(eval) if(eval < 0){perror("eval"); exit(-1);}

// Macros - same as above, but save the result(res) of expression(eval)
#define CHK2(res, eval) if((res = eval) < 0){perror("eval"); exit(-1);}

int strtoint(std::string s);

std::string inttostr(int n);

std::string eetostr(epoll_event ev);

int setnonblocking(int sockfd);

class Log {
private:
    Log() = delete;

    static int level;
    static std::vector<std::ostream *> targets;

    static std::string get_time() {
        time_t now = time(0);
        struct tm tstruct;
        char buf[80];
        tstruct = *localtime(&now);
        strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
        return buf;
    }

    static void print(std::string prefix, std::string message) {
        std::string result_message = get_time() + " [" + prefix + "]: " + message + "\n";

        for (std::ostream *o : targets) {
            (*o) << result_message;
        }
    }

public:
    static void add_output(std::ostream *o) {
        targets.push_back(o);
    }

    static void set_level(int level) {
        Log::level = level;
    }

    static void e(std::string message) {
        if (level < 0) return;
        print("E", message);
    }

    static void d(std::string message) {
        if (level < 1) return;
        print("D", message);
    }

    static void w(std::string message) {
        if (level < 2) return;
        print("W", message);
    }
};

#endif //KIMBERLY_UTIL_H
