#include "proxy_server.h"
#include <signal.h>

std::shared_ptr<proxy_server> s;
std::thread *terminator;

bool caught = false;

void signal_handler(int signum) {
    if (caught) exit(signum);
    caught = true;
    Log::d("Error code: " + inttostr(signum));
    Log::print_stack_trace(6);
    Log::d("resetting");
    s.reset();
    Log::d("joining");
    if (terminator->joinable()) {
        Log::d("really join");
        terminator->join();
    }
    Log::d("before delete");
    delete terminator;
    Log::d("exiting");
    exit(signum);
}

void watch_signal(int sig) {
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = signal_handler;
    sigaction(sig, &action, NULL);
}

pthread_t main_t;

int main() {
    Log::add_output(&(std::cout));
    Log::set_level(2);

    main_t = pthread_self();

    terminator = new std::thread((std::function<void()>) ([]() -> void {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        Log::d("terminating");
        pthread_kill(main_t, SIGINT);
    }));

    watch_signal(SIGQUIT);
    watch_signal(SIGABRT);
    watch_signal(SIGSEGV);
    watch_signal(SIGTERM);
    watch_signal(SIGINT);

    try {
        s = std::make_shared<proxy_server>(std::string("127.0.0.1"), 4501);
        s->loop();
    } catch (const std::exception &e) {
        Log::e("Exception " + std::string(e.what()));
        Log::print_stack_trace(6);
    }

    s.reset();
    terminator->join();
    delete terminator;
    return 0;
}