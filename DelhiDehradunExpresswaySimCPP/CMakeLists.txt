cmake_minimum_required(VERSION 3.20)
project(ExpresswaySim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(expressway_sim
  src/main.cpp
  src/Graph.cpp
  src/FenwickTree.cpp
  src/BloomFilter.cpp
  src/DSU.cpp
)

target_include_directories(expressway_sim PRIVATE include)

add_executable(expressway_server
  src/server_main.cpp
  src/Graph.cpp
  src/FenwickTree.cpp
  src/BloomFilter.cpp
  src/DSU.cpp
)

target_include_directories(expressway_server PRIVATE include third_party)

if (MSVC)
  target_compile_options(expressway_sim PRIVATE /W4 /permissive-)
  target_compile_options(expressway_server PRIVATE /W4 /permissive-)
else()
  target_compile_options(expressway_sim PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(expressway_server PRIVATE -Wall -Wextra -Wpedantic)
endif()
