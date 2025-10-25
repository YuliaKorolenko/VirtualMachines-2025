cmake_minimum_required(VERSION 3.15)
project(HW_2 CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(HW_2 main.cpp
        operation.cpp
        machine_state.h)
