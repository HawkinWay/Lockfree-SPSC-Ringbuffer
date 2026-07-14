cmake_minimum_required(VERSION 3.20)
project(SPSCRingbuffer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(spsc_ring_buffer INTERFACE)

target_include_directories(spsc_ring_buffer INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

option(BUILD_TESTING "Build unit test" ON)
option(BUILD_BENCHMARKS "Build microbenchmarks" ON)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

add_executable(spsc_demo src/main.cpp)
target_link_libraries(spsc_demo PRIVATE spsc_ring_buffer)