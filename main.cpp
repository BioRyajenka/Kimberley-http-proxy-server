#include "proxy_server.h"
#include <iostream>
#include <netdb.h>
#include "util.h"

int main() {
    Log::add_output(&(std::cout));
    Log::set_level(2);

    proxy_server s("127.0.0.1", 4502);
    s.prepare();
    s.loop();
    s.terminate();

    /*std::string s = "HTTP/1.1 302 Found\n"
            "Server: Apache\n"
            "Date: Sat, 02 Apr 2016 15:20:24 GMT\n"
            "Content-Type: text/html; charset=windows-1251\n"
            "Content-Length: 20\n"
            "Connection: keep-alive\n"
            "X-Powered-By: PHP/3.22697\n"
            "Location: /feed\n"
            "Content-Encoding: gzip\n"
            "\n"
            "\u001F�\b";
    std::cout << find_double_line_break(s, 0);*/

    /*struct hostent *he;
    he = gethostbyname("agar.io");
    puts(inet_ntoa(*((struct in_addr *) he->h_addr_list[0])));*/
}