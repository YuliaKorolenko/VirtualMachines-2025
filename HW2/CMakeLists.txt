cmake_minimum_required(VERSION 3.15)
project(HW2 C)

set(CMAKE_C_STANDARD 11)

add_subdirectory(runtime)

add_executable(HW2 stack.c bytefile.c main.c vm.c)

target_link_libraries(HW2 PRIVATE runtime)

target_compile_options(HW2 PRIVATE -O3)

set(PYTHON_EXECUTABLE python3)

enable_testing()

add_test(
        NAME regression_tests
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.py --exe $<TARGET_FILE:HW2>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

set_tests_properties(regression_tests PROPERTIES
        LABELS regression
        TIMEOUT 60
)

add_test(
        NAME performance_tests
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/performance.sh" "$<TARGET_FILE:HW2>"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

set_tests_properties(performance_tests PROPERTIES
        LABELS performance
)
