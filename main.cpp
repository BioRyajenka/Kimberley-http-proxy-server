#include "proxy_server.h"
#include <iostream>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include "util.h"

void signalHandler(int signum) {
    Log::d("Error code: " + inttostr(signum));
    Log::print_stack_trace(10);
    exit(signum);
}

void watch_signal(int sig) {
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = signalHandler;
    sigaction(sig, &action, NULL);
}

int main() {
    Log::add_output(&(std::cerr));
    Log::set_level(2);

    watch_signal(SIGQUIT);
    watch_signal(SIGABRT);
    watch_signal(SIGSEGV);

    proxy_server s("127.0.0.1", 4500);
    try {
        s.loop();
    } catch (const std::exception &e) {
        Log::e("Exception " + std::string(e.what()));
        s.terminate();
    }
}