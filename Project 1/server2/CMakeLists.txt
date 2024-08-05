cmake_minimum_required(VERSION 3.27)
project(server2 C)

set(CMAKE_C_STANDARD 11)

add_executable(server2
        comcli.c
        comserver.c)
