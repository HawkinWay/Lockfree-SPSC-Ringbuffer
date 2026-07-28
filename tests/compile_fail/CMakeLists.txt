add_executable(strTest string_type.cpp)
add_executable(vecTest vector_type.cpp)

target_link_libraries(strTest
    PRIVATE
        spsc_ring_buffer
)

target_link_libraries(vecTest
    PRIVATE
        spsc_ring_buffer
)