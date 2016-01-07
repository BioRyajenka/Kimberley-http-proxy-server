cmake_minimum_required(VERSION 3.3)
project(Kimberly)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp util.h proxy_server.cpp proxy_server.h util.cpp)
add_executable(Kimberly ${SOURCE_FILES})