#pragma once
#include <cstddef>
#include <stdexcept>
#include <atomic>

namespace shovy{

template<typename T>
class RingBuffer{
public:
    explicit RingBuffer(size_t capacity) : capacity_(capacity) {
        if (capacity_ == 0) {
            throw std::invalid_argument("RingBuffer capacity must be > 0");
        }
        buffer_ = new T[capacity_];
    }

    ~RingBuffer() {
        delete[]buffer_;
    }

    // producer
    bool push(const T& item) { 
        if (full())  return false;
        buffer_[write_idx.load() % capacity_] = item;
        write_idx.fetch_add(1);     // write_idx++; is okay
        return true; 
    }

    // consumer
    bool pop(T& item) { 
        if (empty()) return false;
        item = buffer_[read_idx.load() % capacity_];
        read_idx.fetch_add(1);
        return true;
    }

    bool empty() const { return write_idx.load() == read_idx.load(); }
    bool full() const { return size() == capacity_; }
    size_t size() const { return write_idx.load() - read_idx.load(); }
    size_t capacity() const { return capacity_; }

private:
    T* buffer_;
    size_t capacity_;
    std::atomic<size_t> write_idx{0};
    std::atomic<size_t> read_idx{0};
};

}   // namespace shovy