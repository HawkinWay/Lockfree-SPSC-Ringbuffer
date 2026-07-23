#include <benchmark/benchmark.h>
#include <spsc/ring_buffer.hpp>
#include <thread>
#include <atomic>

template<typename Queue>
static void BM_SPSC_Throughput(benchmark::State& state) {
    const size_t capacity = state.range(0);
    // 每次迭代处理的 Ops 数量，10,000,000 次非常适合做长跑吞吐量测试
    constexpr size_t total_operations = 10'000'000;

    for (auto _ : state) {
        // 1. 停止计时：准备测试资源
        state.PauseTiming();

        Queue buffer{capacity};
        std::atomic<bool> start{false};

        // 2. 创建子线程
        std::thread producer([&]() {
            // 线程自旋等待起跑信号
            while (!start.load(std::memory_order_acquire)) {
#if defined(__x86_64__) || defined(_M_X64)
                _mm_pause(); // x86 友好自旋
#endif
            }

            for (size_t i = 0; i < total_operations; ++i) {
                while (!buffer.push(i)) {
                    std::this_thread::yield();
                }
            }
        });

        std::thread consumer([&]() {
            while (!start.load(std::memory_order_acquire)) {
#if defined(__x86_64__) || defined(_M_X64)
                _mm_pause();
#endif
            }

            size_t val = 0;
            for (size_t i = 0; i < total_operations; ++i) {
                while (!buffer.pop(val)) {
                    std::this_thread::yield();
                }
                // 阻止编译器将 pop 出来的 val 优化掉
                benchmark::DoNotOptimize(val);
            }
        });

        // 3. 恢复计时：正式开始发车！
        state.ResumeTiming();

        // 释放屏障，触发双线程同时狂飚
        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();

        // 4. 再次停止计时：线程回收不计入队列性能
        state.PauseTiming();
    }

    // 正确设置处理的 Item 总数 (Push Count + Pop Count = 2 * total_operations)
    state.SetItemsProcessed(state.iterations() * total_operations * 2);
}

// 绑定不同的缓冲区容量测试 (64, 1024, 4096)
BENCHMARK_TEMPLATE(BM_SPSC_Throughput, shovy::RingBuffer<size_t>)
        ->Arg(64)
        ->Arg(1024)
        ->Arg(4096)
        ->UseRealTime()
        ->Repetitions(5)
        ->ReportAggregatesOnly(true); // 只输出 Mean/Median/StdDev，界面更清爽

BENCHMARK_MAIN();