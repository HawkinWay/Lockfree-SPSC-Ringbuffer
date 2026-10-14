#include <benchmark/benchmark.h>

#include <spsc/ring_buffer.hpp>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>
#include <type_traits>
#include <vector>

struct TimeItem {
    uint64_t sequence;
    int64_t pushTimeNs;
    bool sampled;
};

static_assert(std::is_trivially_copyable_v<TimeItem>, "TimeItem must be trivially copyable for RB7");

using Clock = std::chrono::steady_clock;

static int64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
}

static int64_t percentileSorted(const std::vector<int64_t>& sorted, double p) {
    assert(!sorted.empty());
    assert(p >= 0.0 && p <= 1.0);

    const auto rank = static_cast<size_t>(std::ceil(p * sorted.size()));
    return sorted[std::max(size_t{1}, rank) - 1];
}

template <typename Queue>
static void BM_SPSC_Throughput(benchmark::State& state) {
    const auto capacity = static_cast<size_t>(state.range(0));
    constexpr size_t total_operations = 10'000'000;

    Queue buffer{capacity};

    for (auto _ : state) {
        state.PauseTiming();
        std::atomic<bool> start{false};

        std::thread producer([&]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (size_t i = 0; i < total_operations; ++i) {
                while (!buffer.push(i)) {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([&]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            size_t value = 0;
            for (size_t i = 0; i < total_operations; ++i) {
                while (!buffer.pop(value)) {
                    std::this_thread::yield();
                }
                benchmark::DoNotOptimize(value);
            }
        });

        state.ResumeTiming();
        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(total_operations) * 2);
}

template <typename Queue>
static void BM_SPSC_BatchThroughput(benchmark::State& state) {
    const auto capacity = static_cast<size_t>(state.range(0));
    const auto batch_size = static_cast<size_t>(state.range(1));
    constexpr size_t total_operations = 10'000'000;

    Queue buffer{capacity};
    std::vector<size_t> input(batch_size);
    std::vector<size_t> output(batch_size);

    for (size_t i = 0; i < batch_size; ++i) {
        input[i] = i;
    }

    for (auto _ : state) {
        state.PauseTiming();
        std::atomic<bool> start{false};

        std::thread producer([&]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            size_t produced = 0;
            while (produced < total_operations) {
                const size_t request = std::min(batch_size, total_operations - produced);

                if (const size_t pushed = buffer.push_batch(input.data(), request); pushed > 0) {
                    produced += pushed;
                } else {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([&]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            size_t consumed = 0;
            while (consumed < total_operations) {
                const size_t request = std::min(batch_size, total_operations - consumed);

                if (const size_t popped = buffer.pop_batch(output.data(), request); popped > 0) {
                    consumed += popped;
                    benchmark::DoNotOptimize(output.data());
                } else {
                    std::this_thread::yield();
                }
            }
        });

        state.ResumeTiming();
        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(total_operations) * 2);
}

template <typename Queue>
static void BM_SPSC_SaturatedEndToEndLatency(benchmark::State& state) {
    const auto capacity = static_cast<size_t>(state.range(0));
    constexpr uint64_t total_operations = 10'000'000;
    constexpr uint64_t sample_stride = 1000;
    constexpr auto sample_count = static_cast<size_t>((total_operations + sample_stride - 1) / sample_stride);

    Queue buffer{capacity};
    std::vector<int64_t> samples(sample_count);

    int64_t p50{0};
    int64_t p90{0};
    int64_t p95{0};
    int64_t p99{0};
    int64_t max_latency{0};

    for (auto _ : state) {
        state.PauseTiming();
        std::ranges::fill(samples, 0);
        size_t sample_idx{0};
        std::atomic<bool> start{false};

        std::thread producer([&]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (uint64_t i = 0; i < total_operations; ++i) {
                TimeItem item{i, 0, (i % sample_stride == 0)};

                // Refresh the timestamp before each push attempt so time
                // spent waiting for a free slot is not attributed to this item.
                while (true) {
                    if (item.sampled) {
                        item.pushTimeNs = nowNs();
                    }

                    if (buffer.push(item)) {
                        break;
                    }
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([&]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (uint64_t i = 0; i < total_operations; ++i) {
                TimeItem item{};
                while (!buffer.pop(item)) {
                    std::this_thread::yield();
                }

                if (item.sampled) {
                    const int64_t latency_ns = nowNs() - item.pushTimeNs;
                    assert(sample_idx < samples.size());
                    samples[sample_idx++] = latency_ns;
                }
            }
        });

        state.ResumeTiming();
        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();

        state.PauseTiming();
        assert(sample_idx == sample_count);

        std::ranges::sort(samples);
        p50 = percentileSorted(samples, 0.50);
        p90 = percentileSorted(samples, 0.90);
        p95 = percentileSorted(samples, 0.95);
        p99 = percentileSorted(samples, 0.99);
        max_latency = samples.back();

        state.ResumeTiming();
    }

    state.counters["p50_ns"] = static_cast<double>(p50);
    state.counters["p90_ns"] = static_cast<double>(p90);
    state.counters["p95_ns"] = static_cast<double>(p95);
    state.counters["p99_ns"] = static_cast<double>(p99);
    state.counters["max_ns"] = static_cast<double>(max_latency);

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(total_operations) * 2);
}

// Template aliases keep registration readable and avoid long concrete types.
template <typename T> using RB1 = shovy::RingBuffer1<T>;
template <typename T> using RB2 = shovy::RingBuffer2<T>;
template <typename T> using RB3 = shovy::RingBuffer3<T>;
template <typename T> using RB4 = shovy::RingBuffer4<T>;
template <typename T> using RB5 = shovy::RingBuffer5<T>;
template <typename T> using RB6 = shovy::RingBuffer6<T>;
template <typename T> using RB7 = shovy::RingBuffer7<T>;

#define REGISTER_CAPACITY_BENCHMARK(BenchmarkFn, QueueType) \
    BENCHMARK_TEMPLATE(BenchmarkFn, QueueType)                    \
        ->Arg(64)->Arg(1024)->Arg(4096)                            \
        ->UseRealTime()->Repetitions(15)->ReportAggregatesOnly(true)

#define REGISTER_BATCH_BENCHMARK(QueueType)                       \
    BENCHMARK_TEMPLATE(BM_SPSC_BatchThroughput, QueueType)        \
        ->Args({4096, 8})->Args({4096, 32})                        \
        ->Args({4096, 64})->Args({4096, 256})                     \
        ->UseRealTime()->Repetitions(15)->ReportAggregatesOnly(true)

#define REGISTER_LATENCY_BENCHMARK(QueueType)                     \
    BENCHMARK_TEMPLATE(BM_SPSC_SaturatedEndToEndLatency, QueueType) \
        ->Arg(64)->Arg(1024)->Arg(4096)                            \
        ->UseRealTime()->Iterations(1)->Repetitions(15)            \
        ->ReportAggregatesOnly(true)

// Throughput: RB1 is retained as an unsafe reference only; its data races
// make it unsuitable for correctness-preserving performance conclusions.
REGISTER_CAPACITY_BENCHMARK(BM_SPSC_Throughput, RB1<size_t>);
REGISTER_CAPACITY_BENCHMARK(BM_SPSC_Throughput, RB2<size_t>);
REGISTER_CAPACITY_BENCHMARK(BM_SPSC_Throughput, RB3<size_t>);
REGISTER_CAPACITY_BENCHMARK(BM_SPSC_Throughput, RB4<size_t>);
REGISTER_CAPACITY_BENCHMARK(BM_SPSC_Throughput, RB5<size_t>);
REGISTER_CAPACITY_BENCHMARK(BM_SPSC_Throughput, RB6<size_t>);
REGISTER_CAPACITY_BENCHMARK(BM_SPSC_Throughput, RB7<size_t>);

// Batch API exists in RB6 and RB7.
REGISTER_BATCH_BENCHMARK(RB6<size_t>);
REGISTER_BATCH_BENCHMARK(RB7<size_t>);

// Latency: exclude RB1 because it has data races / undefined behavior.
REGISTER_LATENCY_BENCHMARK(RB2<TimeItem>);
REGISTER_LATENCY_BENCHMARK(RB3<TimeItem>);
REGISTER_LATENCY_BENCHMARK(RB4<TimeItem>);
REGISTER_LATENCY_BENCHMARK(RB5<TimeItem>);
REGISTER_LATENCY_BENCHMARK(RB6<TimeItem>);
REGISTER_LATENCY_BENCHMARK(RB7<TimeItem>);

BENCHMARK_MAIN();

