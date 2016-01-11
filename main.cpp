#include "proxy_server.h"
#include <iostream>
#include <netdb.h>
#include "util.h"

int main() {
    Log::add_output(&(std::cout));
    Log::set_level(2);

    proxy_server s("192.168.1.217", 4507);
    s.prepare();
    s.loop();
    s.terminate();
    /*struct hostent *he;
    he = gethostbyname("agar.io");
    puts(inet_ntoa(*((struct in_addr *) he->h_addr_list[0])));*/
}