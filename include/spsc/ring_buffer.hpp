#pragma once
#include <cstddef>
#include <stdexcept>

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
        buffer_[write_idx % capacity_] = item;
        write_idx++;
        return true; 
    }

    // consumer
    bool pop(T& item) { 
        if (empty()) return false;
        item = buffer_[read_idx % capacity_];
        read_idx++;
        return true;
    }

    bool empty() const { return write_idx == read_idx; }
    bool full() const { return size() == capacity_; }
    size_t size() const { return write_idx - read_idx; }
    size_t capacity() const { return capacity_; }

private:
    T* buffer_;
    size_t capacity_;
    size_t write_idx{0};
    size_t read_idx{0};
};

}   // namespace shovy