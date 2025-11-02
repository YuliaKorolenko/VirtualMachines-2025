cmake_minimum_required(VERSION 3.30)
project(HW2 C)

set(RUNTIME_LIB ${CMAKE_CURRENT_SOURCE_DIR}/runtime/runtime.a)

set(CMAKE_C_STANDARD 11)

add_executable(HW2 main.c)

target_link_libraries(HW2 PRIVATE ${RUNTIME_LIB})
