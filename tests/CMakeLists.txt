include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
)

set(gtest_force_shared_ctr ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


add_executable(ring_buffer_test ring_buffer_test.cpp)

target_link_libraries(ring_buffer_test
    PRIVATE
        spsc_ring_buffer
        gtest_main
)

include(GoogleTest)
gtest_discover_tests(ring_buffer_test)