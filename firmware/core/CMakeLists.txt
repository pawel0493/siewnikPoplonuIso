cmake_minimum_required(VERSION 3.16)
project(siewnik_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(seeder_core
    seeder_controller.cpp
)
target_include_directories(seeder_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(seeder_core_test
    tests/seeder_controller_test.cpp
)
target_link_libraries(seeder_core_test PRIVATE seeder_core)

enable_testing()
add_test(NAME seeder_core_test COMMAND seeder_core_test)

