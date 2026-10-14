# SPSC Ring Buffer Benchmark Report

> Data is based on [`benchmark_data/benchmark_final.txt`](benchmark_data/benchmark_final.txt). This report summarizes the currently archived results and does not represent universal performance across all CPUs, compilers, or runtime environments.

## Summary

Six implementations were compared using Google Benchmark. Each configuration was repeated 15 times, and the tables report medians. All variants were run in the same benchmark binary and test environment.

- **RB2**: `seq_cst` atomic operations.
- **RB3**: `acquire`/`release` atomic operations.
- **RB4**: Cache-line alignment added to RB3.
- **RB5**: Bitwise index wrapping for power-of-two capacities added to RB4.
- **RB6**: A `memcpy` fast path for trivially copyable types added to RB5.
- **RB7**: Compile-time constraints added to RB6, including lock-free/trivially-copyable checks and allocator/`operator new` constraints.

*RB1 is the version with ordinary non-atomic indices.*

Key observations:

1. In this test, RB3 achieved approximately **6.4–6.7×** the reported throughput of RB2 at capacities 64, 1024, and 4096.
2. RB6 reached approximately **2.45 G ops/s** in reported throughput at batch size 256. A complete element transfer consists of one push and one pop, so the complete-transfer rate is approximately half the reported value, or **1.23 G ops/s**.
3. Cache-line alignment, bitwise index wrapping, and the `memcpy` fast path did not show a stable, independently attributable throughput improvement across all capacities.
4. Saturated latency results cluster around two main levels, approximately 1.19 µs and 2.38 µs. This suggests that scheduling, retry behavior, or timer resolution may significantly affect the measurements; the current design cannot distinguish nanosecond-scale queue-latency differences.

## Environment and Measurement Method

- **Platform**: Ubuntu 26.04.
- **CPU**: AMD Ryzen 7 (8 cores, 16 threads, Zen 4).
- **Frequency**: 4.82 GHz (reported in the log). This is metadata only and does not mean the frequency remained fixed during the test.
- **Compiler/build**: GCC 11.4, Release, `-O3`.
- **Tool**: Google Benchmark; `Repetitions(15)`, `ReportAggregatesOnly(true)`, and `UseRealTime`; tables report medians.
- **Counting convention**: `SetItemsProcessed` counts each push and each pop as one item. Reported throughput is the combined count of push and pop operations; the complete element-transfer rate is approximately half the reported value.
- **Reciprocal rate**: This is the reciprocal of reported throughput under the counting convention above. It is **not a direct measurement of the latency of a single push/pop or an end-to-end transfer**.
- **Different payloads**: The latency test uses a 24-byte `TimeItem` (sequence number, timestamp, and flag), while the throughput test uses an 8-byte `size_t`. Their absolute values cannot be directly compared.
- **RB1 excluded**: The ordinary non-atomic-index version contains a data race/undefined behavior (UB) and cannot serve as a valid performance baseline.
- **Noise**: Code layout, compiler-generated code, CPU frequency changes, scheduling, and system load may all affect the results. Without additional validation, small differences cannot be attributed to a single optimization.

## Single-Element Throughput

The following are medians over 15 repetitions per configuration. `M/s` means millions of reported items per second. Push and pop are counted separately, so these values are not complete element transfers per second.

| Variant | Capacity | Reported throughput | Reciprocal rate | CV (real time) |
|:---|---:|---:|---:|:---:|
| RB2 — `seq_cst` | 64 | 44.21 M/s | 22.6 ns | 3.0–5.8% |
| | 1024 | 43.47 M/s | 23.0 ns | 3.0% |
| | 4096 | 42.01 M/s | 23.8 ns | 3.0% |
| RB3 — `acquire`/`release` | 64 | 284.60 M/s | 3.51 ns | 2.1% |
| | 1024 | 285.02 M/s | 3.51 ns | 1.5% |
| | 4096 | 282.97 M/s | 3.53 ns | 1.2% |
| RB4 — cache-line alignment | 64 | 285.78 M/s | 3.50 ns | 1.1% |
| | 1024 | 282.30 M/s | 3.54 ns | 0.6% |
| | 4096 | 267.64 M/s | 3.74 ns | 0.5% |
| RB5 — bitwise index wrapping | 64 | 289.50 M/s | 3.45 ns | 0.7% |
| | 1024 | 278.95 M/s | 3.58 ns | 0.9% |
| | 4096 | 266.42 M/s | 3.75 ns | 0.8% |
| RB6 — `memcpy` | 64 | 328.70 M/s | 3.04 ns | 1.1% |
| | 1024 | 291.16 M/s | 3.43 ns | 2.0% |
| | 4096 | 259.02 M/s | 3.86 ns | 1.5% |
| RB7 — compile-time hardening | 64 | 329.36 M/s | 3.04 ns | 1.4% |
| | 1024 | 287.80 M/s | 3.47 ns | 1.8% |
| | 4096 | 257.04 M/s | 3.89 ns | 1.7% |

### Interpretation

1. **Memory ordering shows the largest difference.** RB3 achieved approximately 6.4×, 6.6×, and 6.7× the reported throughput of RB2 at capacities 64, 1024, and 4096, respectively.
2. **Cache-line alignment did not provide a consistent independent throughput gain.** RB4 was faster in some capacities and slower in others relative to RB3, so these results do not justify claiming that padding always improves performance.
3. **Bitwise index wrapping did not show a stable throughput improvement.** Disassembly can confirm changes to the instruction path, but an instruction-level change does not necessarily produce a measurable performance gain.
4. **The effect of `memcpy` depends on the configuration.** RB6 was faster at smaller capacities but slower than RB5 at capacity 4096, so it cannot be described as a universal speedup.
5. **RB7's main value is compile-time constraint enforcement.** The throughput difference between RB7 and RB6 is small and changes direction with capacity. Compile-time checks such as `static_assert` do not add runtime operations; the impact of allocator constraints should be evaluated against the specific code and generated machine code.

## Batch Throughput

Capacity is fixed at 4096. Reported throughput counts push and pop separately; the complete element-transfer rate is approximately half the reported value.

| Batch size | RB6 reported throughput (median) | RB7 reported throughput (median) | CV (RB6) |
|:--:|--:|--:|--:|
| 8 | 551.41 M/s | 570.99 M/s | 15.5% |
| 32 | 1.6379 G/s | 1.6441 G/s | 0.6% |
| 64 | 1.7572 G/s | 1.7711 G/s | 0.5% |
| 256 | 2.4509 G/s | 2.4952 G/s | 1.8% |

Relative to RB6's single-element reported throughput at capacity 4096, batch sizes 8, 32, 64, and 256 achieved approximately **2.1×, 6.3×, 6.8×, and 9.5×**, respectively. Growth is not linear, and larger batches are not guaranteed to continue scaling proportionally.

RB7's batch-throughput advantage over RB6 is approximately 0.4%–3.5%, which is insufficient to establish a stable performance advantage. Note that the coefficient of variation for batch size 8 is relatively high, so this result should be interpreted cautiously.

## Saturated End-to-End Latency

The latency test uses the `TimeItem` data structure and records the interval between a successful enqueue and the corresponding dequeue. The producer and consumer retry with `sched_yield()` when an operation cannot proceed. Approximately 10,000 elements are sampled per run, with 15 runs per configuration.

| Configuration | p50 | p90 | p95 | p99 | max (median run) |
|:--|--:|--:|--:|--:|--:|
| RB2–RB7, most capacities | 1.188 µs | 2.375 µs | 2.375 µs | 2.375 µs | approximately 19–77 µs |

*RB2 at capacity 4096 is an exception: the reported median p99 is approximately 3.56 µs, the mean is approximately 10.3 µs, and the CV is approximately 125%.*

### Latency Limitations

- The concentration of percentiles around approximately 1.19 µs and 2.38 µs suggests that scheduling, retry cadence, or timer resolution may dominate the current measurements.
- These results **do not by themselves prove** that each bucket corresponds exactly to one or two `sched_yield()` round trips, nor do they prove that tail latency is entirely caused by operating-system preemption.
- Nanosecond-scale differences between queue operations are much smaller than the observed microsecond-scale buckets, so the current design is not suitable for distinguishing small latency differences between implementations.
- Future work could use a different retry strategy, a dedicated latency-test design, and queue-occupancy tracking to better distinguish scheduling behavior from queue-residence time.

## Conclusions

- Under the current test conditions, `acquire`/`release` shows the largest single-element throughput difference compared with `seq_cst`.
- The Batch API substantially increases reported throughput, indicating that batch operations can amortize some synchronization and call overhead; the gains diminish as batch size increases.
- Cache-line alignment, bitwise index wrapping, the `memcpy` fast path, and compile-time constraints did not provide stable, independently attributable throughput improvements at every capacity.
- Current latency results are significantly affected by saturated thread interaction and the measurement method, and are insufficient to support fine-grained queue-latency conclusions.

## Known Limitations and Future Work

- Throughput results may be affected by CPU frequency, temperature, system scheduling, compiler version, and code layout.
- The latency test needs further validation of its timing and retry strategy; the current results should not be used for fine-grained latency comparisons.
- A performance benchmark does not itself prove thread safety or real-time safety. See the unit tests for correctness testing.
- For cross-session stability comparisons, preserve the raw results and repeat tests with the same compiler, build flags, and environment. Small-sample observations do not prove a causal relationship between padding and stability.

## Reproducing the Benchmark

The following command selects the `performance` governor to reduce the effect of governor policy on frequency changes (and to eliminate the `CPU scaling is enabled` warning during benchmark builds). Turbo/boost behavior, temperature, and power limits may still cause frequency changes.

```bash
sudo cpupower frequency-set -g performance

cmake -B build/ -DCMAKE_BUILD_TYPE=Release
cmake --build build/ --config Release --target ring_buffer_bench

./build/benchmarks/ring_buffer_bench \\
  --benchmark_out=benchmark_results.json \\
  --benchmark_out_format=json \\
  2>&1 | tee benchmark_console.txt
```
