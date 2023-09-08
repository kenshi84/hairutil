cmake_minimum_required(VERSION 3.0)

project(hairutil)

set(CMAKE_CXX_STANDARD 17)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/args
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/cyCodeBase
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/eigen
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/spdlog/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(hairutil
    src/cmd/convert.cpp
    src/io/bin.cpp
    src/io/data.cpp
    src/io/hair.cpp
    src/io/ma.cpp
    src/globals.cpp
    src/main.cpp
)
