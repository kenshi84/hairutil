cmake_minimum_required(VERSION 3.1...3.31)

project(hairutil)

set(CMAKE_CXX_STANDARD 20)

# Suppress some compiler warnings
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-Wno-deprecated-declarations -Wno-free-nonheap-object -Wno-stringop-overflow)
endif()


#########################################
# External libraries that require build #
#########################################

# Alembic
set(ALEMBIC_SHARED_LIBS OFF CACHE BOOL "Build Alembic as a shared library")
set(USE_BINARIES OFF CACHE BOOL "Include Alembic binaries")
set(USE_TESTS OFF CACHE BOOL "Include Alembic tests")
add_subdirectory(ext/alembic)

# HDF5
set(HDF5_EXTERNALLY_CONFIGURED 1)
set(BUILD_TESTING OFF CACHE BOOL "Build HDF5 tests")
set(HDF5_BUILD_EXAMPLES OFF CACHE BOOL "Build HDF5 Library Examples")
set(HDF5_BUILD_TOOLS OFF CACHE BOOL "Build HDF5 Tools")
set(HDF5_BUILD_PARALLEL_TOOLS OFF CACHE BOOL "Build Parallel HDF5 Tools")
set(BUILD_STATIC_LIBS ON CACHE BOOL "Build HDF5 as a static library")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build HDF5 as a shared library")
add_subdirectory(ext/hdf5)

# xlnt
set(STATIC TRUE CACHE BOOL "Build xlnt as a static library")
add_subdirectory(ext/xlnt)



#########################
# Version tag using git #
#########################
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



######################
# Core functionality #
######################
add_library(hairutil_core STATIC
  src/cmd/autofix.cpp
  src/cmd/convert.cpp
  src/cmd/decompose.cpp
  src/cmd/filter.cpp
  src/cmd/findpenet.cpp
  src/cmd/getcurvature.cpp
  src/cmd/info.cpp
  src/cmd/resample.cpp
  src/cmd/smooth.cpp
  src/cmd/stats.cpp
  src/cmd/subsample.cpp
  src/cmd/transform.cpp
  src/cmd/tubify.cpp
  src/globals.cpp
  src/io/abc.cpp
  src/io/bin.cpp
  src/io/data.cpp
  src/io/hair.cpp
  src/io/ma.cpp
  src/io/npy.cpp
  src/io/ply.cpp
  src/util.cpp
  version.cpp
)
target_link_libraries(hairutil_core
  Alembic::Alembic
  hdf5-static
  xlnt
)
target_include_directories(hairutil_core
  PUBLIC
  include
  ext/args
  ext/cyCodeBase
  ext/eigen
  ext/happly
  ext/HighFive/include
  ext/libigl/include
  ext/libnpy/include
  ext/nanoflann/include
  ext/spdlog/include
)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(hairutil_core PUBLIC -Wno-psabi)
endif()
install(TARGETS hairutil_core DESTINATION lib)

###################
# Main executable #
###################
add_executable(hairutil src/main.cpp)
target_link_libraries(hairutil hairutil_core)
install(TARGETS hairutil DESTINATION bin)



#########################
# Test using GoogleTest #
#########################
add_subdirectory(ext/googletest)
enable_testing()

# Test data directory
set(REL_TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test/data/")
get_filename_component(TEST_DATA_DIR "${REL_TEST_DATA_DIR}" ABSOLUTE)

# IO tests
add_executable(test_io test/io.cpp)
target_link_libraries(test_io hairutil_core gtest_main)
target_compile_definitions(test_io PRIVATE TEST_DATA_DIR="${TEST_DATA_DIR}")
add_test(NAME test_io COMMAND test_io)

# Command tests
add_executable(test_cmd src/main.cpp test/cmd.cpp)
target_link_libraries(test_cmd hairutil_core gtest_main)
target_compile_definitions(test_cmd
  PRIVATE TEST_DATA_DIR="${TEST_DATA_DIR}"
  PRIVATE TEST_MODE=1
)
add_test(NAME test_cmd COMMAND test_cmd)

# Util tests
add_executable(test_util test/util.cpp)
target_link_libraries(test_util hairutil_core gtest_main)
add_test(NAME test_util COMMAND test_util)
