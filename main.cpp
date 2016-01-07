#include "proxy_server.h"
#include <iostream>
#include "util.h"

int main() {
    Log::add_output(&(std::cout));
    Log::set_level(2);

    proxy_server s("192.168.0.30", 4502);
    s.prepare();
    s.loop();
    s.terminate();
}