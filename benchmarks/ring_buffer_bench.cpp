#include <benchmark/benchmark.h>
#include <spsc/ring_buffer.hpp>
#include <thread>
#include <atomic>

template<typename Queue>
static void BM_SPSC_Throughput(benchmark::State& state) {
    const size_t capacity = state.range(0);
    constexpr size_t total_operations = 10'000'000;

    for (auto _ : state) {
        state.PauseTiming();

        Queue buffer{capacity};
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

        state.PauseTiming();
    }

    state.SetItemsProcessed(state.iterations() * total_operations * 2);
}

template<typename Queue>
static void BM_SPSC_BatchThroughput(benchmark::State& state) {
    const size_t capacity = state.range(0);
    constexpr size_t total_operations = 10'000'000;
    const size_t batch_size = state.range(1);

    for (auto _ : state) {
        state.PauseTiming();

        Queue buffer{capacity};
        std::atomic<bool> start{false};

        std::vector<size_t> input(batch_size);
        std::vector<size_t> output(batch_size);
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

        state.PauseTiming();
    }

    state.SetItemsProcessed(
        state.iterations() *
        total_operations *
        2
    );
}

BENCHMARK_TEMPLATE(BM_SPSC_Throughput, shovy::RingBuffer<size_t>)
        ->Arg(64)
        ->Arg(1024)
        ->Arg(4096)
        ->UseRealTime()
        ->Repetitions(5)
        ->ReportAggregatesOnly(true);

BENCHMARK_TEMPLATE(BM_SPSC_BatchThroughput, shovy::RingBuffer<size_t>)
        ->Args({4096, 8})
        ->Args({4096, 32})
        ->Args({4096, 64})
        ->Args({4096, 256})
        ->UseRealTime()
        ->Repetitions(5)
        ->ReportAggregatesOnly(true);

BENCHMARK_MAIN();