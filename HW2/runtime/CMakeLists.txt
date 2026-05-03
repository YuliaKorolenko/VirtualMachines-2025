cmake_minimum_required(VERSION 3.15)

set(RUNTIME_SOURCES
        gc.c
        runtime.c
)

add_library(runtime STATIC ${RUNTIME_SOURCES})

set_target_properties(runtime PROPERTIES
        C_STANDARD 11
)

target_compile_options(runtime PRIVATE
        -Wno-shift-negative-value
        -g
        -fstack-protector-all
)

if(APPLE)
    set(CMAKE_C_COMPILER clang)
endif()

if(UNIX AND NOT APPLE)
    set(CMAKE_C_COMPILER gcc)
endif()

target_compile_definitions(runtime PRIVATE LAMA_ENV)

target_include_directories(runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_source_files_properties(printf.S PROPERTIES
        LANGUAGE C
        COMPILE_FLAGS "-Wa,--noexecstack -x assembler-with-cpp -g"
)