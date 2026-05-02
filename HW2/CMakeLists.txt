cmake_minimum_required(VERSION 3.30)
project(HW2 C)

set(CMAKE_C_STANDARD 11)

add_subdirectory(runtime)

add_executable(HW2 stack.c bytefile.c main.c vm.c)

target_link_libraries(HW2 PRIVATE runtime)

target_compile_options(HW2 PRIVATE -O3)