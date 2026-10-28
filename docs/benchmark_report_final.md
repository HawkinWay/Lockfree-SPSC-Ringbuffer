# SPSC Ring Buffer Benchmark Results

> Archived data: October 26, 2026. This document lists implementation differences, measurement data, and numerical observations.
>
> Raw data: [throughput.json](benchmark_data/archived/throughput.json), [latency.json](benchmark_data/archived/latency.json), [environment snapshot](benchmark_data/archived/env_snapshot.txt), [CTest log](benchmark_data/archived/ctest.log).

## 1. Implementations

| Variant | Implementation recorded in source |
|---|---|
| RB2 | Atomic indices use default sequentially consistent ordering. Single-element push/pop use `fetch_add`; availability checks call `full()`/`empty()`. |
| RB3 | Uses explicit relaxed/acquire/release loads and stores for the indices. Index updates use calculated next values and `store` instead of `fetch_add`. |
| RB4 | Separately aligns the read and write atomic indices using `std::hardware_destructive_interference_size`. |
| RB5 | Requires power-of-two capacity and uses bitwise masking for index wraparound instead of modulo. |
| RB6 | Adds a trivially-copyable type constraint and batch `push_batch`/`pop_batch` operations implemented with `memcpy`. |
| RB7 | Adds cached remote-index values for single-element `push`/`pop`, uses `memcpy` for single-element copies, allocates raw storage with `::operator new`, and adds compile-time constraints. |
| RB7_2 | Uses cached remote-index values and the `memcpy` single-element path. The queue type is aligned with `alignas(64)`, while the read and write indices do not each have separate alignment declarations. |

## 2. Test Environment and Method

| Property | Archived record |
|---|---|
| CPU | AMD Ryzen 7 7840H with Radeon 780M Graphics |
| CPU topology | 8 cores / 16 logical CPUs |
| Kernel | Linux 7.0.0-38-generic |
| Build configuration | Release |
| CPU governor | `performance` |
| CPU boost | Enabled |
| Clocksource | `hpet`; available: `hpet`, `acpi_pm` |
| Producer / consumer affinity | CPU 0 / CPU 2 |
| Environment snapshot timestamp | 2026-10-26 20:35:17 +0800 |
| Snapshot commit field | `a2ec19d-dirty` |
| Throughput JSON timestamp | 2026-10-26 20:35:20 +0800 |
| Latency JSON timestamp | 2026-10-26 20:40:21 +0800 |

Single-element throughput and batch throughput each use 15 repetitions. The benchmark processed-item counter counts transferred elements. The throughput table therefore uses **million elements transferred per second (M elements/s)**.

CV is population standard deviation divided by the mean across the 15 per-run measurements, expressed as a percentage.

The ping-pong latency benchmark performs 100,000 round trips per run, skips 1,000 warm-up round trips, and samples every tenth round trip (approximately 9,900 samples per run). It measures the interval from the producer's initial enqueue until the echoed token returns, then divides the round-trip interval by two to estimate one-way handoff latency. The latency table reports the median of each per-run percentile across 15 repetitions. The final column is the median of the per-run maximums.

## 3. Single-Element Throughput

Median throughput in M elements/s; CV shown in parentheses.

| Variant | Capacity 64 | Capacity 1024 | Capacity 4096 |
|---|---:|---:|---:|
| RB2 | 15.8 (0.2%) | 16.3 (1.6%) | 15.8 (0.3%) |
| RB3 | 146.4 (0.1%) | 142.1 (1.4%) | 145.0 (0.2%) |
| RB4 | 148.7 (0.2%) | 150.2 (0.6%) | 144.9 (0.5%) |
| RB5 | 172.1 (0.3%) | 155.9 (0.4%) | 141.1 (0.5%) |
| RB6 | 172.6 (0.2%) | 155.1 (0.6%) | 140.8 (0.3%) |
| RB7 | 323.7 (0.2%) | 172.0 (2.8%) | 154.1 (0.3%) |
| RB7_2 | 218.7 (0.5%) | 249.3 (2.4%) | 238.4 (0.4%) |

### Numeric observations

- RB3 / RB2 throughput ratio: **9.27×** at capacity 64, **8.72×** at capacity 1024, and **9.18×** at capacity 4096.
- RB7 / RB2 throughput ratio: **20.49×** at capacity 64, **10.55×** at capacity 1024, and **9.75×** at capacity 4096.
- At capacity 64, RB7 recorded **323.7 M elements/s**, the highest value in this single-element table.
- At capacities 1024 and 4096, RB7_2 recorded **249.3 M** and **238.4 M elements/s**, respectively.
- Across the seven variants, listed single-element throughput CV values range from **0.1% to 2.8%**.

## 4. Batch Throughput

Capacity is 4096 elements. Values are median M elements/s across 15 repetitions; CV is shown separately.

| Batch size | RB6 median | RB6 CV | RB7 median | RB7 CV |
|---:|---:|---:|---:|---:|
| 8 | 312.3 | 12.7% | 322.1 | 11.8% |
| 32 | 874.6 | 8.3% | 875.5 | 13.7% |
| 64 | 914.3 | 14.0% | 913.5 | 15.7% |
| 256 | 1261.2 | 9.6% | 1374.3 | 3.2% |

### Numeric observations

- RB7 batch throughput rises from **322.1 M elements/s** at batch size 8 to **1374.3 M elements/s** at batch size 256.
- For RB7, the batch-size-256 result is approximately **8.92×** its single-element result at capacity 4096 (`1374.3 / 154.1`).
- At batch size 32, RB6 and RB7 report **874.6 M** and **875.5 M elements/s**.
- At batch size 64, RB6 and RB7 report **914.3 M** and **913.5 M elements/s**.
- At batch size 256, RB7 reports **1374.3 M elements/s**, compared with **1261.2 M elements/s** for RB6.
- The listed CV values range from **3.2% to 15.7%**; the lowest is RB7 at batch size 256.

## 5. Ping-Pong Latency

Values are nanoseconds. Each percentile is the median of that percentile across 15 runs. “Median per-run max” is the median of the maximum sample from each run.

| Variant | Capacity | p50 | p90 | p95 | p99 | Median per-run max |
|---|---:|---:|---:|---:|---:|---:|
| RB2 | 64 | 115 | 130 | 130 | 145 | 8836 |
| RB2 | 1024 | 120 | 135 | 140 | 155 | 8901 |
| RB2 | 4096 | 120 | 135 | 140 | 150 | 8891 |
| RB3 | 64 | 110 | 115 | 115 | 130 | 8896 |
| RB3 | 1024 | 110 | 115 | 115 | 135 | 8641 |
| RB3 | 4096 | 110 | 115 | 115 | 130 | 8370 |
| RB4 | 64 | 85 | 95 | 100 | 115 | 8566 |
| RB4 | 1024 | 85 | 95 | 100 | 115 | 8295 |
| RB4 | 4096 | 85 | 95 | 100 | 120 | 8641 |
| RB5 | 64 | 85 | 90 | 95 | 110 | 8801 |
| RB5 | 1024 | 85 | 90 | 95 | 105 | 8380 |
| RB5 | 4096 | 85 | 90 | 95 | 110 | 8135 |
| RB6 | 64 | 85 | 90 | 95 | 105 | 8556 |
| RB6 | 1024 | 85 | 90 | 95 | 105 | 8661 |
| RB6 | 4096 | 85 | 90 | 95 | 110 | 8475 |
| RB7 | 64 | 80 | 90 | 90 | 110 | 8586 |
| RB7 | 1024 | 80 | 90 | 90 | 105 | 8801 |
| RB7 | 4096 | 80 | 90 | 90 | 110 | 8821 |
| RB7_2 | 64 | 110 | 120 | 125 | 135 | 9167 |
| RB7_2 | 1024 | 110 | 120 | 125 | 135 | 8886 |
| RB7_2 | 4096 | 110 | 120 | 125 | 135 | 9032 |

### Numeric observations

- RB7's p50 is **80 ns** at all three tested capacities.
- RB7's p99 is **105 ns** at capacity 1024 and **110 ns** at capacities 64 and 4096.
- RB7_2's p50 is **110 ns** at all three tested capacities.
- The median per-run maximum values in this table range from **8135 ns to 9167 ns**.
- For the same capacity, RB7's listed p50 is **30 ns lower** than RB7_2's at capacities 64, 1024, and 4096.

## 6. Archived Correctness Test Result

The archived `ctest.log` records **11 tests passed, 0 tests failed**.

## 7. Data Files

- [Archived throughput JSON](benchmark_data/archived/throughput.json)
- [Archived latency JSON](benchmark_data/archived/latency.json)
- [Archived environment snapshot](benchmark_data/archived/env_snapshot.txt)
- [Archived CTest output](benchmark_data/archived/ctest.log)
- [Archived generated data tables](benchmark_data/archived/data.md)
