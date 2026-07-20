#include <gtest/gtest.h>
#include <spsc/ring_buffer.hpp>
#include <vector>
#include <thread>

TEST(RingBufferTest, InitialCapacityMatches){
    shovy::RingBuffer<int> buffer(128);
    EXPECT_EQ(buffer.capacity(), 128);
}

// single thread test
TEST(RingBufferTest, BasicOperations){
    shovy::RingBuffer<int> rb(4);

    EXPECT_EQ(rb.capacity(), 4);
    EXPECT_TRUE(rb.empty());
    EXPECT_FALSE(rb.full());

    EXPECT_TRUE(rb.push(10));
    EXPECT_TRUE(rb.push(20));
    EXPECT_EQ(rb.size(), 2);
    EXPECT_TRUE(rb.push(30));
    EXPECT_TRUE(rb.push(40));

    EXPECT_TRUE(rb.full());
    EXPECT_FALSE(rb.push(50));
    EXPECT_EQ(rb.size(), 4);

    int val = 0;
    EXPECT_TRUE(rb.pop(val));
    EXPECT_EQ(val, 10);
    EXPECT_EQ(rb.size(), 3);

    EXPECT_TRUE(rb.push(50));
    EXPECT_TRUE(rb.full());
}

TEST(RingBufferTest, MultiThreadDataRaceDemonstration){
    const size_t count = 10'000'000;
    shovy::RingBuffer<size_t> buffer(1024);

    std::thread producer([&](){
        for(size_t i = 0; i < count; i++){
            while(!buffer.push(i)){
                std::this_thread::yield();
            }
        }
    });

    std::vector<size_t> received;
    received.reserve(count);
    std::thread consumer([&](){
        size_t val = 0;
        for(int i = 0; i < count; i++){
            while(!buffer.pop(val)){
                std::this_thread::yield();
            }
            received.push_back(val);
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), count);

    for(size_t i = 0; i < count; i++){
        EXPECT_EQ(received[i], i) << "Data race detected at index" << i;
        if(received[i] != i){
            FAIL() << "Terminating test early due to data race/corruption.";
        }
    }

}