#include <benchmark/benchmark.h>

#include <spsc/ring_buffer.hpp>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <type_traits>
#include <vector>

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
#include <immintrin.h>
#endif

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

static inline void cpuRelax() noexcept {
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
    _mm_pause();
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("yield" ::: "memory");
#else
    std::this_thread::yield();
#endif
}

static void pinCurrentThread(int cpu) {
#if defined(__linux__)
    if (cpu < 0) return;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (pthread_setaffinity_np(pthread_self(), sizeof(set), &set) != 0) {
        static std::atomic<bool> warned{false};
        if (!warned.exchange(true, std::memory_order_relaxed)) {
            std::fprintf(stderr, "[bench] warning: could not pin thread to CPU %d "
                                 "(cgroup/cpuset restriction?)\n", cpu);
        }
    }
#else
    (void)cpu;   // pinning is only implemented on Linux
#endif
}

static int cpuFromEnv(const char* name, int fallback) {
#if defined(__linux__)
    if (const char* s = std::getenv(name)) {
        const int v = std::atoi(s);
        if (v >= 0) return v;
    }
#else
    (void)name;
#endif
    return fallback;
}

struct TimeItem {
    uint64_t sequence;
    int64_t pushTimeNs;
    bool sampled;
};

static_assert(std::is_trivially_copyable_v<TimeItem>, "TimeItem must be trivially copyable for RB7");

using Clock = std::chrono::steady_clock;

struct LatencyClock {
    static uint64_t cyclesNow() noexcept {
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
        uint32_t lo, hi, aux;
        __asm__ __volatile__(
            "lfence\n\t"
            "rdtscp\n\t"    // cpuid + rdtsc
            "lfence"
            : "=a"(lo), "=d"(hi), "=c"(aux)
            :
            : "memory");
        return (static_cast<uint64_t>(hi) << 32) | lo;
#else
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now().time_since_epoch()).count());
#endif
    }

    static double nsPerCycle() {
        static const double factor = [] {
            const auto t0 = Clock::now();
            const uint64_t c0 = cyclesNow();
            while (std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count() < 200) {
                cpuRelax();
            }
            const uint64_t c1 = cyclesNow();
            const auto t1 = Clock::now();
            const auto wall_ns = static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
            const auto cycles = static_cast<double>(c1 - c0);
            return cycles > 0.0 ? wall_ns / cycles : 1.0;
        }();
        return factor;
    }

    static int64_t cyclesToNs(uint64_t cycles) {
        return static_cast<int64_t>(static_cast<double>(cycles) * nsPerCycle());
    }
};

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
    const int producer_cpu = cpuFromEnv("SPSC_PRODUCER_CPU", 0);
    const int consumer_cpu = cpuFromEnv("SPSC_CONSUMER_CPU", 2);

    Queue buffer{capacity};

    for (auto _ : state) {
        state.PauseTiming();
        std::atomic<bool> start{false};

        std::thread producer([&]() {
            pinCurrentThread(producer_cpu);
            while (!start.load(std::memory_order_acquire)) {
                cpuRelax();
            }

            for (size_t i = 0; i < total_operations; ++i) {
                while (!buffer.push(i)) {
                    cpuRelax();
                }
            }
        });

        std::thread consumer([&]() {
            pinCurrentThread(consumer_cpu);
            while (!start.load(std::memory_order_acquire)) {
                cpuRelax();
            }

            size_t value = 0;
            for (size_t i = 0; i < total_operations; ++i) {
                while (!buffer.pop(value)) {
                    cpuRelax();
                }
                benchmark::DoNotOptimize(value);
            }
        });

        state.ResumeTiming();
        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();
    }

    state.counters["sizeof_queue"] = static_cast<double>(sizeof(Queue));
    state.counters["obj_mod64"] = static_cast<double>(reinterpret_cast<std::uintptr_t>(&buffer) % 64);
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(total_operations));
}

template <typename Queue>
static void BM_SPSC_BatchThroughput(benchmark::State& state) {
    const auto capacity = static_cast<size_t>(state.range(0));
    const auto batch_size = static_cast<size_t>(state.range(1));
    constexpr size_t total_operations = 10'000'000;
    const int producer_cpu = cpuFromEnv("SPSC_PRODUCER_CPU", 0);
    const int consumer_cpu = cpuFromEnv("SPSC_CONSUMER_CPU", 2);

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
            pinCurrentThread(producer_cpu);
            while (!start.load(std::memory_order_acquire)) {
                cpuRelax();
            }

            size_t produced = 0;
            while (produced < total_operations) {
                const size_t request = std::min(batch_size, total_operations - produced);

                if (const size_t pushed = buffer.push_batch(input.data(), request); pushed > 0) {
                    produced += pushed;
                } else {
                    cpuRelax();
                }
            }
        });

        std::thread consumer([&]() {
            pinCurrentThread(consumer_cpu);
            while (!start.load(std::memory_order_acquire)) {
                cpuRelax();
            }

            size_t consumed = 0;
            while (consumed < total_operations) {
                const size_t request = std::min(batch_size, total_operations - consumed);

                if (const size_t popped = buffer.pop_batch(output.data(), request); popped > 0) {
                    consumed += popped;
                    benchmark::DoNotOptimize(output[0]);
                } else {
                    cpuRelax();
                }
            }
        });

        state.ResumeTiming();
        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();
    }

    state.counters["sizeof_queue"] = static_cast<double>(sizeof(Queue));
    state.counters["obj_mod64"] = static_cast<double>(reinterpret_cast<std::uintptr_t>(&buffer) % 64);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(total_operations));
}

template <typename Queue>
static void BM_SPSC_RoundTripLatency(benchmark::State& state) {
    const auto capacity = static_cast<size_t>(state.range(0));
    constexpr uint64_t round_trips = 100'000;
    constexpr uint64_t warmup_round_trips = 1'000;
    constexpr uint64_t sample_stride = 10;

    const int main_cpu = cpuFromEnv("SPSC_MAIN_CPU", 0);
    const int consumer_cpu = cpuFromEnv("SPSC_CONSUMER_CPU", 2);
    pinCurrentThread(main_cpu);

    Queue to_consumer{capacity};
    Queue to_producer{capacity};
    std::vector<int64_t> samples;
    samples.reserve((round_trips - warmup_round_trips) / sample_stride);

    int64_t p50{0};
    int64_t p90{0};
    int64_t p95{0};
    int64_t p99{0};
    int64_t max_latency{0};

    for (auto _ : state) {
        state.PauseTiming();
        samples.clear();
        std::atomic<bool> start{false};

        std::thread consumer([&]() {
            pinCurrentThread(consumer_cpu);
            while (!start.load(std::memory_order_acquire)) {
                cpuRelax();
            }

            TimeItem item{};
            for (uint64_t i = 0; i < round_trips; ++i) {
                while (!to_consumer.pop(item)) {
                    cpuRelax();
                }
                while (!to_producer.push(item)) {
                    cpuRelax();
                }
            }
        });

        state.ResumeTiming();
        start.store(true, std::memory_order_release);

        TimeItem item{};
        for (uint64_t i = 0; i < round_trips; ++i) {
            item.sequence = i;
            item.sampled = (i >= warmup_round_trips) && (i % sample_stride == 0);

            const uint64_t t0 = LatencyClock::cyclesNow();
            while (!to_consumer.push(item)) {
                cpuRelax();
            }

            TimeItem echo{};
            while (!to_producer.pop(echo)) {
                cpuRelax();
            }
            const uint64_t t1 = LatencyClock::cyclesNow();

            if (echo.sampled) {
                samples.push_back(LatencyClock::cyclesToNs(t1 - t0) / 2);
            }
        }

        consumer.join();

        state.PauseTiming();
        assert(!samples.empty());

        std::ranges::sort(samples);
        p50 = percentileSorted(samples, 0.50);
        p90 = percentileSorted(samples, 0.90);
        p95 = percentileSorted(samples, 0.95);
        p99 = percentileSorted(samples, 0.99);
        max_latency = samples.back();

        state.ResumeTiming();
    }

    state.counters["sizeof_queue"] = static_cast<double>(sizeof(Queue));
    state.counters["obj_mod64"] = static_cast<double>(reinterpret_cast<std::uintptr_t>(&to_consumer) % 64);
    state.counters["p50_ns"] = static_cast<double>(p50);
    state.counters["p90_ns"] = static_cast<double>(p90);
    state.counters["p95_ns"] = static_cast<double>(p95);
    state.counters["p99_ns"] = static_cast<double>(p99);
    state.counters["max_ns"] = static_cast<double>(max_latency);
    state.counters["tsc_ns_per_cycle"] = LatencyClock::nsPerCycle();
}

template <typename T> using RB1 = shovy::RingBuffer1<T>;
template <typename T> using RB2 = shovy::RingBuffer2<T>;
template <typename T> using RB3 = shovy::RingBuffer3<T>;
template <typename T> using RB4 = shovy::RingBuffer4<T>;
template <typename T> using RB5 = shovy::RingBuffer5<T>;
template <typename T> using RB6 = shovy::RingBuffer6<T>;
template <typename T> using RB7 = shovy::RingBuffer7<T>;
template <typename T> using RB7_2 = shovy::RingBuffer7_2<T>;

#define REGISTER_CAPACITY_BENCHMARK(BenchmarkFn, QueueType) \
    BENCHMARK_TEMPLATE(BenchmarkFn, QueueType)                    \
        ->Arg(64)->Arg(1024)->Arg(4096)                            \
        ->UseRealTime()->Repetitions(15)->DisplayAggregatesOnly(true)

#define REGISTER_BATCH_BENCHMARK(QueueType)                       \
    BENCHMARK_TEMPLATE(BM_SPSC_BatchThroughput, QueueType)        \
        ->Args({4096, 8})->Args({4096, 32})                        \
        ->Args({4096, 64})->Args({4096, 256})                     \
        ->UseRealTime()->Repetitions(15)->DisplayAggregatesOnly(true)

#define REGISTER_LATENCY_BENCHMARK(QueueType)                     \
    BENCHMARK_TEMPLATE(BM_SPSC_RoundTripLatency, QueueType)         \
        ->Arg(64)->Arg(1024)->Arg(4096)                            \
        ->UseRealTime()->Iterations(1)->Repetitions(15)            \
        ->DisplayAggregatesOnly(true)


REGISTER_CAPACITY_BENCHMARK(BM_SPSC_Throughput, RB2<size_t>);
REGISTER_CAPACITY_BENCHMARK(BM_SPSC_Throughput, RB3<size_t>);
REGISTER_CAPACITY_BENCHMARK(BM_SPSC_Throughput, RB4<size_t>);
REGISTER_CAPACITY_BENCHMARK(BM_SPSC_Throughput, RB5<size_t>);
REGISTER_CAPACITY_BENCHMARK(BM_SPSC_Throughput, RB6<size_t>);
REGISTER_CAPACITY_BENCHMARK(BM_SPSC_Throughput, RB7<size_t>);
REGISTER_CAPACITY_BENCHMARK(BM_SPSC_Throughput, RB7_2<size_t>);

REGISTER_BATCH_BENCHMARK(RB6<size_t>);
REGISTER_BATCH_BENCHMARK(RB7<size_t>);


REGISTER_LATENCY_BENCHMARK(RB2<TimeItem>);
REGISTER_LATENCY_BENCHMARK(RB3<TimeItem>);
REGISTER_LATENCY_BENCHMARK(RB4<TimeItem>);
REGISTER_LATENCY_BENCHMARK(RB5<TimeItem>);
REGISTER_LATENCY_BENCHMARK(RB6<TimeItem>);
REGISTER_LATENCY_BENCHMARK(RB7<TimeItem>);
REGISTER_LATENCY_BENCHMARK(RB7_2<TimeItem>);

BENCHMARK_MAIN();


