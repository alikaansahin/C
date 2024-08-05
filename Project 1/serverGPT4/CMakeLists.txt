cmake_minimum_required(VERSION 3.27)
project(serverGPT4 C)

set(CMAKE_C_STANDARD 11)

add_executable(serverGPT4
        comcli.c
        comserver.c)
