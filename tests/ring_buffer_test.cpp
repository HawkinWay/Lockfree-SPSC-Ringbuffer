#include <gtest/gtest.h>
#include <spsc/ring_buffer.hpp>

TEST(RingBufferTest, InitialCapacityMatches){
    shovy::RingBuffer<int> buffer(128);
    EXPECT_EQ(buffer.capacity(), 128);
}