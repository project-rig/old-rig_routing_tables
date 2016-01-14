cmake_minimum_required (VERSION 2.8)
project("Rig Routing Table Tools")

# Set C++14 mode, warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -pedantic")

# Add the desktop utility
add_subdirectory(desktop)

# Compile the tests
add_subdirectory(tests)
