cmake_minimum_required(VERSION 3.5)

project(hairutil)

set(CMAKE_CXX_STANDARD 17)

include_directories(
    ext/args
    ext/cyCodeBase
    ext/eigen
    ext/happly
    ext/nanoflann/include
    ext/spdlog/include
    include
)

add_subdirectory(ext/alembic)

set(VERSIONTAG "unknown")
find_package(Git QUIET)
if (GIT_FOUND OR Git_FOUND)
  execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD RESULT_VARIABLE RET OUTPUT_VARIABLE COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE)
  if (NOT RET)
    set(VERSIONTAG "${COMMIT}")
  endif()
endif()
message(STATUS "Version tag: ${VERSIONTAG}")
configure_file("src/version.cpp.in" "version.cpp")

add_executable(hairutil
    src/cmd/convert.cpp
    src/cmd/decompose.cpp
    src/cmd/info.cpp
    src/cmd/resample.cpp
    src/cmd/subsample.cpp
    src/io/abc.cpp
    src/io/bin.cpp
    src/io/data.cpp
    src/io/hair.cpp
    src/io/ma.cpp
    src/io/ply.cpp
    src/globals.cpp
    src/main.cpp
    version.cpp
)

target_link_libraries(hairutil
    Alembic::Alembic
)
