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

# Do not include test
set(USE_TESTS OFF CACHE BOOL "Include Alembic tests")
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

add_library(hairutil_cmd
  src/cmd/autofix.cpp
  src/cmd/convert.cpp
  src/cmd/decompose.cpp
  src/cmd/info.cpp
  src/cmd/resample.cpp
  src/cmd/subsample.cpp
  src/cmd/transform.cpp
)

add_library(hairutil_io
  src/io/abc.cpp
  src/io/bin.cpp
  src/io/data.cpp
  src/io/hair.cpp
  src/io/ma.cpp
  src/io/ply.cpp
)
target_link_libraries(hairutil_io
  Alembic::Alembic
)

add_executable(hairutil
  src/globals.cpp
  src/main.cpp
  version.cpp
)
target_link_libraries(hairutil
  hairutil_cmd
  hairutil_io
)

# Test
add_subdirectory(ext/googletest)
enable_testing()

# Test data directory
set(REL_TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test/data/")
get_filename_component(TEST_DATA_DIR "${REL_TEST_DATA_DIR}" ABSOLUTE)

# IO tests
add_executable(test_io
  src/globals.cpp
  test/io.cpp
)
target_link_libraries(test_io
  hairutil_io
  gtest_main
)
target_compile_definitions(test_io
  PRIVATE TEST_DATA_DIR="${TEST_DATA_DIR}"
)
add_test(NAME test_io COMMAND test_io)

# Command tests
add_executable(test_cmd
  src/globals.cpp
  src/main.cpp
  test/cmd.cpp
  version.cpp
)
target_link_libraries(test_cmd
  hairutil_cmd
  hairutil_io
  gtest_main
)
target_compile_definitions(test_cmd
  PRIVATE TEST_DATA_DIR="${TEST_DATA_DIR}"
  PRIVATE TEST_MODE=1
)
add_test(NAME test_cmd COMMAND test_cmd)

add_executable(test_util
  test/util.cpp
)
target_link_libraries(test_util
  gtest_main
)
add_test(NAME test_util COMMAND test_util)
