#include "proxy_server.h"
#include <iostream>
#include "util.h"

int main() {
    Log::add_output(&(std::cout));
    Log::set_level(2);

    proxy_server s("192.168.1.234", 4500);
    s.prepare();
    s.loop();
    s.terminate();
    //idea test commit
}