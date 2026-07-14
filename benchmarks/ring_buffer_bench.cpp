#include <benchmark/benchmark.h>
#include <spsc/ring_buffer.hpp>

static void BM_BufferCreation(benchmark::State& state){
    for(auto _ : state){
        shovy::RingBuffer<int> buffer(1024);
        benchmark::DoNotOptimize(buffer); 
    }
}

BENCHMARK(BM_BufferCreation);

BENCHMARK_MAIN();