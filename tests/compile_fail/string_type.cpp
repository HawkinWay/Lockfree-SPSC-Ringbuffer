#include <spsc/ring_buffer.hpp>
#include <string>

int main()
{
    shovy::RingBuffer<std::string> buffer(64);
}