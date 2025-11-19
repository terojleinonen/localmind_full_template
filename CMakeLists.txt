cmake_minimum_required(VERSION 3.16)
project(LocalMind LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

add_compile_options(-Wall -Wextra -Wpedantic)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third_party
)

file(GLOB SRC_FILES src/*.cpp)

add_executable(localmind_server ${SRC_FILES})
