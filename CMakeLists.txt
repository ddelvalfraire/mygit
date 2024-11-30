cmake_minimum_required(VERSION 3.29.3)
project(mygit C)

set(CMAKE_C_STANDARD 99)

# Find uthash include directory
find_path(UTHASH_INCLUDE_DIR
    NAMES uthash.h
    PATHS /usr/local/include
          /opt/homebrew/include
)

if(NOT UTHASH_INCLUDE_DIR)
    message(FATAL_ERROR "uthash.h not found")
endif()

# Add include directory
include_directories(
    ${UTHASH_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    )

# Add executable
add_executable(mygit src/main.c)