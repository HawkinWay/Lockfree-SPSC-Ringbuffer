#include <spsc/ring_buffer.hpp>
#include <iostream>

int main(){
    shovy::RingBuffer<int> buffer(64);
    std::cout << "SPSC Ring Buffer Initialized. Capacity: "
        << buffer.capacity() << "\n";
    return 0;
}