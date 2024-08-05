cmake_minimum_required(VERSION 3.28)
project(proje3_hoca C)

set(CMAKE_C_STANDARD 11)

include_directories(cs342spring2024-p3-main)

add_executable(proje3_hoca
        cs342spring2024-p3-main/app1-2.c
        cs342spring2024-p3-main/app1.c
        cs342spring2024-p3-main/app2.c
        cs342spring2024-p3-main/consumer.c
        cs342spring2024-p3-main/mf.c
        cs342spring2024-p3-main/mf.h
        cs342spring2024-p3-main/mfserver.c
        cs342spring2024-p3-main/producer.c)
