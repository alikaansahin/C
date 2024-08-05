cmake_minimum_required(VERSION 3.28)
project(project3_new C)

set(CMAKE_C_STANDARD 11)

add_executable(project3_new
        client.c
        mf.c
        mf.h
        mfserver.c
        client.c)
