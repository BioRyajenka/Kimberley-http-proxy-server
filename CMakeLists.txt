cmake_minimum_required(VERSION 3.3)
project(Kimberly)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -rdynamic -lpthread -O0 -g -Wall -fsanitize=undefined")

set(SOURCE_FILES main.cpp file_descriptor.h my_addrinfo.h util.h proxy_server.cpp proxy_server.h util.cpp handler.cpp handler.h buffer.cpp buffer.h concurrent_queue.h hostname_resolver.h)
add_executable(Kimberly ${SOURCE_FILES})