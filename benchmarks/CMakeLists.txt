include(FetchContent)

FetchContent_Declare(
    googlebenchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(ring_buffer_bench ring_buffer_bench.cpp)

target_link_libraries(ring_buffer_bench
    PRIVATE
        spsc_ring_buffer
        benchmark::benchmark
)