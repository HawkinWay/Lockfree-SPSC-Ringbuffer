#include <benchmark/benchmark.h>
#include <spsc/ring_buffer.hpp>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdint>

struct TimeItem{
    uint64_t sequence;
    int64_t pushTimeNs;
    bool sampled;
};

using Clock = std::chrono::steady_clock;

static int64_t nowNs(){
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
}

static int64_t percentile(std::vector<int64_t> samples, double p) {
    std::sort(samples.begin(), samples.end());

    const size_t index = static_cast<size_t>(p * (samples.size() - 1));

    return samples[index];
}


template<typename Queue>
static void BM_SPSC_Throughput(benchmark::State& state) {
    const size_t capacity = state.range(0);
    constexpr size_t total_operations = 10'000'000;

    Queue buffer{capacity};

    for (auto _ : state) {
        state.PauseTiming();

        std::atomic<bool> start{false};

        std::thread producer([&]() {
            while (!start.load(std::memory_order_acquire)) {

            }

            for (size_t i = 0; i < total_operations; ++i) {
                while (!buffer.push(i)) {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([&]() {
            while (!start.load(std::memory_order_acquire)) {

            }

            size_t val = 0;
            for (size_t i = 0; i < total_operations; ++i) {
                while (!buffer.pop(val)) {
                    std::this_thread::yield();
                }
                benchmark::DoNotOptimize(val);
            }
        });

        state.ResumeTiming();

        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();

        // state.PauseTiming();
    }

    state.SetItemsProcessed(state.iterations() * total_operations * 2);
}


template<typename Queue>
static void BM_SPSC_BatchThroughput(benchmark::State& state) {
    const size_t capacity = state.range(0);
    constexpr size_t total_operations = 10'000'000;
    const size_t batch_size = state.range(1);

    Queue buffer{capacity};
    std::vector<size_t> input(batch_size);
    std::vector<size_t> output(batch_size);

    for (auto _ : state) {
        state.PauseTiming();

        std::atomic<bool> start{false};

        for (size_t i = 0; i < batch_size; i++){
            input[i] = i;
        }

        std::thread producer([&]() {
            while (!start.load(std::memory_order_acquire)) {

            }

            size_t produced = 0;

            while (produced < total_operations) {
                size_t pushed = buffer.push_batch(
                    input.data(),
                    batch_size
                );

                if (pushed > 0){
                    produced += pushed;
                }
                else{
                    std::this_thread::yield();
                }
            }
        });


        std::thread consumer([&]() {
            while (!start.load(std::memory_order_acquire)) {

            }

            size_t consumed = 0;

            while (consumed < total_operations) {

                size_t popped = buffer.pop_batch(
                    output.data(),
                    batch_size
                );

                if (popped > 0) {
                    consumed += popped;
                    benchmark::DoNotOptimize(output);
                }
                else {
                    std::this_thread::yield();
                }
            }
        });

        state.ResumeTiming();

        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();

        // state.PauseTiming();
    }

    state.SetItemsProcessed(state.iterations() * total_operations * 2);
}

template<typename Queue>
static void BM_SPSC_Latency(benchmark::State& state){
    const size_t capacity = state.range(0);
    constexpr size_t total_operations = 10'000'000;
    constexpr uint64_t sample_stride = 1000;

    std::vector<int64_t> samples;
    Queue buffer{capacity};

    for(auto _ : state){
        std::atomic<bool> start{false};

        std::thread producer([&](){
            while(!start.load(std::memory_order_acquire)){

            }

            for(int i = 0; i < total_operations; i++){
                TimeItem timeItem = {
                    .sequence = i,
                    .pushTimeNs = 0,
                    .sampled = i % sample_stride == 0 ,
                };

                while(true){
                    if(timeItem.sampled){
                        timeItem.pushTimeNs = nowNs();
                    }

                    if(buffer.push(timeItem)){
                        break;
                    }

                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([&](){
            while(!start.load(std::memory_order_acquire)){

            }

            size_t sampleIdx{0}; 

            for(int i = 0; i < total_operations; i++){
                TimeItem timeItem{};
                
                while(!buffer.pop(timeItem)){
                    std::this_thread::yield();
                }

                if(timeItem.sampled){
                    const int64_t latencyNs = nowNs() - timeItem.pushTimeNs;
                    samples[sampleIdx++] = latencyNs; 
                }
            }
        });

        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();


    }
}



BENCHMARK_TEMPLATE(BM_SPSC_Throughput, shovy::RingBuffer1<size_t>)
        ->Arg(64)
        ->Arg(1024)
        ->Arg(4096)
        ->UseRealTime()
        ->Repetitions(15)
        ->ReportAggregatesOnly(true);

BENCHMARK_TEMPLATE(BM_SPSC_Throughput, shovy::RingBuffer2<size_t>)
        ->Arg(64)
        ->Arg(1024)
        ->Arg(4096)
        ->UseRealTime()
        ->Repetitions(15)
        ->ReportAggregatesOnly(true);

BENCHMARK_TEMPLATE(BM_SPSC_Throughput, shovy::RingBuffer3<size_t>)
        ->Arg(64)
        ->Arg(1024)
        ->Arg(4096)
        ->UseRealTime()
        ->Repetitions(15)
        ->ReportAggregatesOnly(true);

BENCHMARK_TEMPLATE(BM_SPSC_Throughput, shovy::RingBuffer4<size_t>)
        ->Arg(64)
        ->Arg(1024)
        ->Arg(4096)
        ->UseRealTime()
        ->Repetitions(15)
        ->ReportAggregatesOnly(true);

BENCHMARK_TEMPLATE(BM_SPSC_Throughput, shovy::RingBuffer5<size_t>)
        ->Arg(64)
        ->Arg(1024)
        ->Arg(4096)
        ->UseRealTime()
        ->Repetitions(15)
        ->ReportAggregatesOnly(true);

BENCHMARK_TEMPLATE(BM_SPSC_Throughput, shovy::RingBuffer6<size_t>)
        ->Arg(64)
        ->Arg(1024)
        ->Arg(4096)
        ->UseRealTime()
        ->Repetitions(15)
        ->ReportAggregatesOnly(true);

BENCHMARK_TEMPLATE(BM_SPSC_BatchThroughput, shovy::RingBuffer6<size_t>)
        ->Args({4096, 8})
        ->Args({4096, 32})
        ->Args({4096, 64})
        ->Args({4096, 256})
        ->UseRealTime()
        ->Repetitions(15)
        ->ReportAggregatesOnly(true);

BENCHMARK_TEMPLATE(BM_SPSC_Latency, shovy::RingBuffer6<TimeItem>)
        ->Arg(64)
        ->Arg(1024)
        ->Arg(4096)
        ->UseRealTime()
        ->Repetitions(15);

BENCHMARK_MAIN();