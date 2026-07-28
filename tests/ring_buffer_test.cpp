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

TEST(RingBufferTest, RejectNonPowerOfTwoCapacity){
    EXPECT_THROW(
	shovy::RingBuffer<int> buffer(100),
	std::invalid_argument
    );
}

TEST(RingBufferTest, AcceptPowerOfTwoCapacity){
    EXPECT_NO_THROW(
	shovy::RingBuffer<int> buffer(1024);
    );
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

TEST(RingBufferTest, BasicBatchOperations){
	shovy::RingBuffer<int> buffer(16);
	EXPECT_EQ(buffer.capacity(), 16);
	EXPECT_TRUE(buffer.empty());

	int data[8] = {0,1,2,3,4,5,6,7};
	int out[8] = {0};

	EXPECT_EQ(buffer.push_batch(data, 8), 8);
	EXPECT_FALSE(buffer.full());
	EXPECT_FALSE(buffer.empty());

	EXPECT_EQ(buffer.pop_batch(out, 8), 8);
	EXPECT_TRUE(buffer.empty());

	for(int i = 0; i < 8; i++){
		EXPECT_EQ(out[i], data[i]);
	}

}

TEST(RingBufferTest, BatchWrapAround) {
    shovy::RingBuffer<int> buffer(16);

    int dummy[14] = {};
    int out[14] = {};
    buffer.push_batch(dummy, 14);
    buffer.pop_batch(out, 14);

    int input[8] = {10,20,30,40,50,60,70,80};
    EXPECT_EQ(buffer.push_batch(input, 8), 8);

    int output[8] = {};
    EXPECT_EQ(buffer.pop_batch(output, 8), 8);

    for(int i = 0; i < 8; i++) {
        EXPECT_EQ(output[i], input[i]);
    }
}


TEST(RingBufferTest, BatchPushPartialWhenFull){
	shovy::RingBuffer<int> buffer(8);

	int input[16] = {};

	EXPECT_EQ(buffer.push_batch(input, 16), 8);
	EXPECT_TRUE(buffer.full());

	EXPECT_EQ(buffer.push_batch(input, 4), 0);
}

TEST(RingBufferTest, BatchPopPartialWhenEmpty){
	shovy::RingBuffer<int> buffer(8);

	int input[4] = {1,2,3,4};
	int output[8] = {};

	buffer.push_batch(input, 4);

	EXPECT_EQ(buffer.pop_batch(output, 8), 4);
	EXPECT_TRUE(buffer.empty());
}

TEST(RingBufferTest, SupportsTriviallyCopyableTypes){
	shovy::RingBuffer<int> intBuffer(64);
	shovy::RingBuffer<float> floatBuffer(64);
	shovy::RingBuffer<size_t> sizeBuffer(64);

	EXPECT_EQ(intBuffer.capacity(), 64);
	EXPECT_EQ(floatBuffer.capacity(), 64);
	EXPECT_EQ(sizeBuffer.capacity(), 64);
}

TEST(RingBufferTest, SupportsAudioFrame)
{
    struct AudioFrame {
        float left;
        float right;
    };

    shovy::RingBuffer<AudioFrame> buffer(64);

    EXPECT_EQ(buffer.capacity(), 64);

    static_assert(std::is_trivially_copyable_v<AudioFrame>);
    static_assert(!std::is_trivially_copyable_v<std::string>);
    static_assert(!std::is_trivially_copyable_v<std::vector<int>>);
}