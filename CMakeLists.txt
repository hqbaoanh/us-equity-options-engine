cmake_minimum_required(VERSION 3.16)
project(options_pricing_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# on in CI
option(ENABLE_SANITIZERS "Build with AddressSanitizer and UBSan" OFF)
if(ENABLE_SANITIZERS)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# can't combine TSan with ASan, separate build
option(ENABLE_TSAN "Build with ThreadSanitizer" OFF)
if(ENABLE_TSAN)
  add_compile_options(-fsanitize=thread)
  add_link_options(-fsanitize=thread)
endif()

add_library(options_core
  src/black_scholes.cpp
  src/binomial.cpp
  src/implied_vol.cpp
  src/batch.cpp
)
target_include_directories(options_core PUBLIC include)
target_compile_options(options_core PRIVATE -Wall -Wextra -Wpedantic)

find_package(Threads REQUIRED)
target_link_libraries(options_core PUBLIC Threads::Threads)

add_executable(bench_batch bench/bench_batch.cpp)
target_link_libraries(bench_batch PRIVATE options_core)

add_executable(convergence bench/convergence.cpp)
target_link_libraries(convergence PRIVATE options_core)

# python module goes into python/ so the scripts can import it
option(BUILD_PYTHON "Build the options_engine Python module" OFF)
if(BUILD_PYTHON)
  set_target_properties(options_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
  find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
  find_package(pybind11 CONFIG REQUIRED)
  pybind11_add_module(options_engine python/bindings.cpp)
  target_link_libraries(options_engine PRIVATE options_core)
  set_target_properties(options_engine PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python)
endif()

# googletest
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

add_executable(tests
  tests/test_black_scholes.cpp
  tests/test_binomial.cpp
  tests/test_american.cpp
  tests/test_greeks.cpp
  tests/test_implied_vol.cpp
  tests/test_batch.cpp
)
target_link_libraries(tests PRIVATE options_core GTest::gtest_main)
gtest_discover_tests(tests)
