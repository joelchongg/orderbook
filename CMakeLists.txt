cmake_minimum_required(VERSION 3.20)
project(OrderBook CXX)

set(CMAKE_CXX_STANDARD 20) # Currently use C++20, may change to C++23
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library of core sources
add_library(orderbook_lib
    src/matching_engine.cpp
    src/order_gateway.cpp
    src/orderbook.cpp
)

# Public headers location
target_include_directories(orderbook_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Fetch Catch2
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.6.0
)
FetchContent_MakeAvailable(Catch2)

# Tests target
enable_testing()
add_executable(orderbook_tests
    tests/test_orderbook.cpp
)

target_link_libraries(orderbook_tests PRIVATE
    orderbook_lib
    Catch2::Catch2WithMain
)

# Discover tests (ctest integration)
include(Catch)
catch_discover_tests(orderbook_tests)


# Run tests via ctest 
# cmake --build build && ctest --test-dir build --output-on-failure  