#include "proxy_server.h"
#include <signal.h>

std::shared_ptr<proxy_server> s;

bool term = false;

void signal_handler(int signum) {
    //std::cout << "[][][][][][]cout error code " << signum << std::endl;
    Log::d("Error code: " + inttostr(signum));
    Log::print_stack_trace(6);
    if (term) {
        Log::d("terminating");
        return;
    }
    term = true;
    s->terminate();
}

void watch_signal(int sig) {
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = signal_handler;
    sigaction(sig, &action, NULL);
}

int main() {
    Log::add_output(&(std::cout));
    Log::set_level(2);

    /*watch_signal(SIGQUIT);
    watch_signal(SIGABRT);
    watch_signal(SIGSEGV);
    watch_signal(SIGTERM);*/
    watch_signal(SIGINT);

    try {
        s = std::make_shared<proxy_server>(std::string("127.0.0.1"), 4500);
        s->loop();
    } catch (const std::exception &e) {
        Log::e("Exception " + std::string(e.what()));
        Log::print_stack_trace(6);
    }

    Log::d("finish");

    // necessary, because ~proxy_server() uses public static Log
    s.reset();
    Log::d("main finish");
    return 0;
}