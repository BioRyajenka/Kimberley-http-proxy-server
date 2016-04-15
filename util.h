//
// Created by root on 07.01.16.
//

#ifndef KIMBERLY_UTIL_H
#define KIMBERLY_UTIL_H

#include <iostream>
#include <fcntl.h>
#include <vector>
#include <sys/epoll.h>
#include <stdexcept>
#include <execinfo.h>
#include <unistd.h>
#include "handler.h"

// Macros - exit in any error (eval < 0) case
#define CHK(eval) if(eval < 0){perror("CHK1"); exit(-1);}

// Macros - same as above, but save the result(res) of expression(eval)
#define CHK2(res, eval) if((res = eval) < 0){perror("CHK2"); exit(-1);}

int strtoint(std::string);

std::string chartostr(const char &);

std::string inttostr(int);

int hextoint(const std::string &);

std::string eetostr(const epoll_event &);

int setnonblocking(const int &sockfd);

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

        fflush(stdout);
        fflush(stderr);
        std::cerr.flush();
        for (std::ostream *o : targets) {
            o->flush();
            (*o) << result_message;
            o->flush();
        }
        std::cerr.flush();
        fflush(stdout);
        fflush(stderr);
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

    static void fatal(std::string message) {
        e(message);
        throw std::runtime_error(message);
    }

    static void print_stack_trace(int depth) {
        void *trace[depth];
        int trace_size = backtrace(trace, depth);

        //trace[1] = (void *) ctx.eip;
        char **messages = backtrace_symbols(trace, trace_size);
        /* skip first stack frame (points here) */
        printf("[bt] Execution path:\n");
        for (int i = 1; i < trace_size; ++i) {
            printf("[bt] #%d %s\n", i, messages[i]);

            /* find first occurence of '(' or ' ' in message[i] and assume
             * everything before that is the file name. (Don't go beyond 0 though
             * (string terminator)*/
            size_t p = 0;
            while (messages[i][p] != '(' && messages[i][p] != ' '
                   && messages[i][p] != 0)
                ++p;

            char syscom[256];
            sprintf(syscom, "addr2line %p -e %.*s", trace[i], p, messages[i]);
            //last parameter is the file name of the symbol
            system(syscom);
        }


        fflush(stderr);
    }
};

bool extract_header(const std::string &s, int to, std::string name, std::string &result);

int find_double_line_break(const std::string &s, int from);

std::string extract_method(const std::string &s);

void set_status_line(std::string &s, const std::string &status_line);

#endif //KIMBERLY_UTIL_H