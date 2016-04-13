#include "proxy_server.h"
#include <iostream>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include "util.h"

void signalHandler(int signum) {
    Log::d("hello :)");
    exit(signum);
}

void signalHandler2(int signum) {
    Log::e("Segmentation fault");
    Log::print_stack_trace(10);
    exit(signum);
}

int main() {
    Log::add_output(&(std::cout));
    Log::set_level(2);

    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = signalHandler;
    sigaction(SIGQUIT, &action, NULL);

    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = signalHandler2;
    sigaction(SIGSEGV, &action, NULL);

    proxy_server s("127.0.0.1", 4500);
    try {
        s.prepare();
        s.loop();
    } catch (const std::exception &e) {
        Log::e("Exception " + std::string(e.what()));
        s.terminate();
    }
}