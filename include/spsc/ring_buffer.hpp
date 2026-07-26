#pragma once
#include <cstddef>
#include <stdexcept>
#include <atomic>
#include <new>

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
