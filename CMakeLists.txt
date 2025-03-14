cmake_minimum_required(VERSION 3.10)
project(ECE141-Archive)

set(CMAKE_CXX_STANDARD 17)

# Include GTest
include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Include directories
include_directories(.)

# Main archive executable
add_executable(archive
        Archive.cpp
        Archive.hpp
        main.cpp
        Testable.hpp
        Testing.hpp
        Timer.hpp
        Tracker.hpp)

# Test executable using GTest
add_executable(tests
        Archive.cpp
        Archive.hpp
        Testing.cpp
        Testable.hpp
        Testing.hpp
        Timer.hpp
        Tracker.hpp)

target_link_libraries(tests gtest gtest_main)
add_test(NAME ArchiveTests COMMAND tests)
