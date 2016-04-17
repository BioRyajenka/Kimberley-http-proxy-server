cmake_minimum_required(VERSION 3.3)
project(Kimberly)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -rdynamic -lpthread")

set(SOURCE_FILES main.cpp util.h proxy_server.cpp proxy_server.h util.cpp handler.cpp handler.h buffer.cpp buffer.h hostname_resolver.cpp hostname_resolver.h concurrent_queue.h)
add_executable(Kimberly ${SOURCE_FILES})