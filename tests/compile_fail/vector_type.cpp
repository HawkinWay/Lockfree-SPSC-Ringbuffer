#include <spsc/ring_buffer.hpp>
#include <vector>

int main()
{
    shovy::RingBuffer<std::vector<int>> buffer(64);
}