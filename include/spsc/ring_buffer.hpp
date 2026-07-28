#pragma once
#include <cstddef>
#include <stdexcept>
#include <atomic>
#include <new>
#include <cstring>
#include <algorithm>

namespace shovy{

template<typename T>
class RingBuffer{
public:
    explicit RingBuffer(size_t capacity) : capacity_(capacity) {
        if (capacity_ == 0 || (capacity_ & (capacity_ - 1)) != 0) {
            throw std::invalid_argument("RingBuffer capacity must be > 0 and a power of two");
        }
        buffer_ = new T[capacity_];
    }

    ~RingBuffer() {
        delete[]buffer_;
    }

    // producer
    bool push(const T& item) {
        const size_t current_w = write_idx.load(std::memory_order_relaxed);
        const size_t current_r = read_idx.load(std::memory_order_acquire);
        if (current_w - current_r == capacity_)  return false;
        buffer_[current_w & (capacity_ - 1)] = item;
        write_idx.store(current_w + 1, std::memory_order_release);
        return true; 
    }

    // consumer
    bool pop(T& item) {
        const size_t current_r = read_idx.load(std::memory_order_relaxed);
        const size_t current_w = write_idx.load(std::memory_order_acquire);
        if (current_w == current_r) return false;
        item = buffer_[current_r & (capacity_ - 1)];
        read_idx.store(current_r + 1, std::memory_order_release);
        return true;
    }

    size_t push_batch(const T *src, size_t count) {
        const size_t current_w = write_idx.load(std::memory_order_relaxed);
        const size_t current_r = read_idx.load(std::memory_order_acquire);
        size_t available = capacity_ - (current_w - current_r);
        size_t writeNum = std::min(count, available);
        
        if (writeNum == 0)      return 0;
        const size_t startW = current_w & (capacity_ - 1);
        const size_t length1 = std::min(capacity_ - startW, writeNum);
        std::memcpy(buffer_ + startW, src, length1 * sizeof(T));

        if (writeNum > length1) {
            const size_t length2 = writeNum - length1;
            std::memcpy(buffer_, src + length1, length2 * sizeof(T));
        }

        write_idx.store(current_w + writeNum, std::memory_order_release);
        return writeNum;
    }

    size_t pop_batch(T* dest, size_t count) {
        const size_t current_r = read_idx.load(std::memory_order_relaxed);
        const size_t current_w = write_idx.load(std::memory_order_acquire);
        size_t available = current_w - current_r;
        size_t readNum = std::min(available, count);

        if (readNum == 0)    return 0;
        const size_t startR = current_r & (capacity_ - 1);
        const size_t length1 = std::min(capacity_ - startR, readNum);
        std::memcpy(dest, buffer_ + startR, length1 * sizeof(T));

        if (readNum > length1) {
            const size_t length2 = readNum - length1;
            std::memcpy(dest + length1, buffer_, length2 * sizeof(T));
        }

        read_idx.store(current_r + readNum, std::memory_order_release);
        return readNum;
    }

    bool empty() const {
        return write_idx.load(std::memory_order_relaxed) == read_idx.load(std::memory_order_relaxed);
    }

    bool full() const { return size() == capacity_; }

    size_t size() const {
        // Approximate size in concurrent context.
        // Not linearizable.
        return write_idx.load(std::memory_order_relaxed) - read_idx.load(std::memory_order_relaxed);
    }

    size_t capacity() const { return capacity_; }

private:
    T* buffer_;
    size_t capacity_;
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> write_idx{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> read_idx{0};
};

}   // namespace shovy
