cmake_minimum_required(VERSION 3.27)
project(server C)

set(CMAKE_C_STANDARD 11)

add_executable(untitled1 comcli.c
        comserver.c)
