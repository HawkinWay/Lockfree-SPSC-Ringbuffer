#pragma once
#include <cstddef>

namespace shovy{

template<typename T>
class RingBuffer{
public:
    explicit RingBuffer(size_t capacity) : capacity_(capacity) {}

    // Dummy API for initial testing
    bool push(const T& item) { return true; }
    bool pop(T& item) { return false; }

    size_t capacity() const { return capacity_; }

private:
    size_t capacity_;
};

}   // namespace shovy