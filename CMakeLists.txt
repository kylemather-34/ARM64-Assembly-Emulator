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

# Task 1: Parser
add_executable(parser
  src/parser_main.cpp
  src/parser.cpp
)
target_include_directories(parser PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Task 2: Registers
add_executable(registers
  src/registers_main.cpp
  src/registers.cpp
)
target_include_directories(registers PRIVATE ${CMAKE_SOURCE_DIR}/include)
 # Task 3: Stack
add_executable(stack
  src/stack_main.cpp
  src/stack.cpp
)
target_include_directories(stack PRIVATE ${CMAKE_SOURCE_DIR}/include)
