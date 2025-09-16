cmake_minimum_required(VERSION 3.15)
project(arm64_parser CXX)

# Ask for C++17 in a compiler-agnostic way
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Common warnings per-compiler
if (MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wconversion -Wshadow)
endif()

add_executable(parser
  src/main.cpp
  src/parser.cpp
)
target_include_directories(parser PRIVATE ${CMAKE_SOURCE_DIR}/include)
