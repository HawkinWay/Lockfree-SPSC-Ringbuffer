# Benchmark Report

> **Platform Update**: All benchmark results below have been migrated to a unified test environment.  
> The previous Apple M4 data is deprecated due to platform inconsistencies.  
> **Test Platform**: AMD Ryzen 7 (16 cores, 5.13 GHz, Ubuntu Linux)  
> **Compiler**: GCC 11.4 (`-O3` - Release)  
> **Benchmark Tool**: Google Benchmark (with 5 repetitions per configuration)

---

## v0.1 Foundation

### 📈 SPSC RingBuffer Baseline (Issue #1)

* **Memory Order**: None (plain `size_t` indices, intentionally unsafe)
* **Alignment**: None
* **Status**: ❌ Contains data races – only for performance reference

| Capacity | Operation Latency (avg) | Throughput (ops/s) | 5-run Std. Dev. (CV) |
|:---------|:------------------------|:-------------------|:---------------------|
| **64**   | ~9.10 ns                | **109.87 M/s**     | ±2.39 M/s (2.17%)    |
| **1024** | ~6.78 ns                | **147.59 M/s**     | ±0.92 M/s (0.62%)    |
| **4096** | ~6.91 ns                | **144.84 M/s**     | ±0.81 M/s (0.56%)    |

> ℹ️ This baseline represents the theoretical upper bound without any synchronization. It is not thread‑safe but provides a ceiling for subsequent optimizations.

---

### 📈 SPSC RingBuffer Atomic SeqCst (Issue #2)

* **Memory Order**: `std::memory_order_seq_cst` (default)
* **Alignment**: None (potential false sharing)

| Capacity | Operation Latency (avg) | Throughput (ops/s) | 5-run Std. Dev. (CV) |
|:---------|:------------------------|:-------------------|:---------------------|
| **64**   | ~24.60 ns               | **40.66 M/s**      | ±1.78 M/s (4.37%)    |
| **1024** | ~28.89 ns               | **34.62 M/s**      | ±0.75 M/s (2.17%)    |
| **4096** | ~28.97 ns               | **34.52 M/s**      | ±0.67 M/s (1.95%)    |

### 👀 Observation

Replacing plain indices with `std::atomic` using the default `seq_cst` ordering introduces a **massive synchronization penalty**.

- Throughput drops by **~76%** (from 147.6 M/s to 34.6 M/s at capacity 1024).
- Latency increases by a factor of **4–5×**.

This clearly motivates the use of **weaker memory ordering** (`acquire`/`release`) and **cache‑line optimization**.

---

## v0.2 Optimization

### 📈 SPSC RingBuffer Acquire‑Release (Issue #3)

* **Memory Order**: `acquire` for loads, `release` for stores (no `seq_cst`)
* **Alignment**: None (default)

| Capacity | Operation Latency (avg) | Throughput (ops/s) | 5-run Std. Dev. (CV) |
|:---------|:------------------------|:-------------------|:---------------------|
| **64**   | ~9.09 ns                | **110.03 M/s**     | ±2.61 M/s (2.38%)    |
| **1024** | ~6.46 ns                | **154.87 M/s**     | ±2.61 M/s (1.69%)    |
| **4096** | ~6.71 ns                | **149.11 M/s**     | ±2.34 M/s (1.57%)    |

### 📈 SPSC RingBuffer Cache Line Alignment (Issue #4)

* **Memory Order**: `acquire`/`release` (same as #3)
* **Alignment**: `alignas(std::hardware_destructive_interference_size)` (64 B) – separates `head` and `tail`

| Capacity | Operation Latency (avg) | Throughput (ops/s) | 5-run Std. Dev. (CV) |
|:---------|:------------------------|:-------------------|:---------------------|
| **64**   | ~11.11 ns               | **89.98 M/s**      | ±2.56 M/s (2.84%)    |
| **1024** | ~6.43 ns                | **155.60 M/s**     | ±1.92 M/s (1.23%)    |
| **4096** | ~6.68 ns                | **149.81 M/s**     | ±4.28 M/s (2.86%)    |

### 📈 SPSC RingBuffer Power-of-Two Bitwise AND (Issue #5)

* **Memory Order**: `acquire`/`release` (same as #3)
* **Alignment**: `alignas(std::hardware_destructive_interference_size)` (same as #4)
* **Key Optimization**: Replaced modulo operation % capacity with bitwise AND & (capacity - 1), requiring capacity to be a power of two


| Capacity | Operation Latency (avg) | Throughput (ops/s) | 5-run Std. Dev. (CV) |
|:---------|:------------------------|:-------------------|:---------------------|
| **64**   | ~12.34 ns               | **81.038 M/s**      | ±2.03 M/s (2.51%)    |
| **1024** | ~5.34 ns                | **187.27 M/s**     | ±4.09 M/s (2.18%)    |
| **4096** | ~5.87 ns                | **170.23 M/s**     | ±15.26 M/s (8.96%)    |

### 👀 Observation

Significant throughput gains at large capacities: At capacity 1024, throughput jumps from 155.6 M/s (#4) to 187.27 M/s—a ~20.3% improvement. At capacity 4096, it improves from 149.8 M/s to 170.26 M/s (~13.6% gain). This clearly demonstrates the benefit of eliminating integer division (div instruction), which typically takes 20–30 CPU cycles, whereas & completes in a single cycle.

Regression at small capacity (64): Throughput drops from 89.98 M/s to 81.03 M/s (~10% decrease). Possible explanations:

- At tiny capacities, the buffer frequently hits full/empty states, where branch misprediction overhead dominates and masks the gain from bitwise operations.
- Poor cache prefetching at small buffer sizes may cause the CPU to stall while waiting for memory coherence, negating the benefit of faster arithmetic.
- The high CV (8.96%) at capacity 4096 indicates greater performance variability under large buffers, possibly due to system load or CPU dynamic frequency scaling.

Engineering trade-off: This optimization requires capacity to be a power of two (otherwise & (capacity - 1) produces incorrect results). This is a classic space-for-time strategy—ideal for performance-critical scenarios where buffer sizes can be pre-aligned (e.g., network packet pools, memory pools). Applications requiring arbitrary prime capacities must retain the modulo operator.

---

## v0.3 Industrial-Ready

### 📈 SPSC RingBuffer Batch Operations and `std::memcpy` (Issue #6)

* **Memory Order**: `acquire`/`release` (same as #3)
* **Alignment**: `alignas(std::hardware_destructive_interference_size)` (same as #4)
* **Capacity**: Power‑of‑two (enables bitwise & for index wrapping)
* **New APIs**: `push_batch(const T*, size_t)` and `pop_batch(T*, size_t)` using `std::memcpy` for `trivially copyable` types

#### Single‑Element Throughput (for reference)
| Capacity | Operation Latency (avg) | Throughput (ops/s) | 5-run Std. Dev. (CV) |
|:---------|:------------------------|:-------------------|:---------------------|
| **64**   | ~11.67 ns               | **85.04 M/s**      | ±1.41 M/s (1.66%)    |
| **1024** | ~6.99 ns                | **189.61 M/s**     | ±3.92 M/s (2.07%)    |
| **4096** | ~6.73 ns                | **180.54 M/s**     | ±5.62 M/s (3.11%)    |
> These numbers are taken from the same binary that also runs the batch benchmarks. They show that adding the batch APIs did not degrade single‑element performance.

#### Batch Throughput (fixed capacity = 4096)
| Capacity | Operation Latency (avg) | Throughput (ops/s) | 5-run Std. Dev. (CV) |
|:---------|:------------------------|:-------------------|:---------------------|
| **8**    | ~12.39 ns               | **394.28 M/s**     | ±4.50 M/s (1.14%)    |
| **64**   | ~6.13 ns               | **1.0197 G/s**     | ±10.77 M/s (1.06%)   |
| **1024** | ~6.21 ns                | **1.2588 G/s**     | ±10.75 M/s (0.85%)   |
| **4096** | ~5.98 ns                | **1.3059 G/s**     | ±18.78 M/s (1.44%)   |

### 👀 Observation

**Batch operations dramatically improve throughput** – even with a small batch of 8, we already see more than 2× the throughput of the single‑element path (394 M/s vs 180 M/s at capacity 4096). With batch size 256, throughput reaches 1.3 G/s, an improvement of ~7×.

**Diminishing returns** – the gain from batch size 64 to 256 is only ~4%, suggesting that the overhead of atomic operations and memory copying has saturated the memory bus. The sweet spot for this hardware is around 64 elements per batch.

**Stability** – the coefficient of variation (CV) remains below 1.5% for all batch sizes, indicating that batch operations exhibit very consistent performance, even under system load.

**Latency per element drops** – from ~12.4 ns (batch=8) down to ~6 ns (batch=32–256), confirming that amortising atomic updates and using memcpy effectively reduces per‑element overhead.

**No regression in single‑element path** – the single‑element throughput (180 M/s) is slightly higher than the previous power‑of‑two version (170 M/s), likely due to the more efficient internal implementation that also benefits the single‑element calls.

---

## 🔍 Analysis

### 1. Acquire‑Release vs. SeqCst
- After switching to `acquire`/`release` (Issue #3), throughput **recovers fully** and even **slightly exceeds** the baseline (154.9 M/s vs. 147.6 M/s at capacity 1024).
- Latency drops back to ~6–9 ns, proving that `seq_cst`’s global ordering is overly pessimistic for the SPSC pattern.

### 2. Cache‑Line Alignment
- Aligning `head` and `tail` to separate cache lines (Issue #4) shows **mixed results**:
  - For capacity 64, throughput decreases (89.98 M/s vs. 110.03 M/s). This may be due to increased padding that negatively affects cache locality for small buffers.
  - For larger capacities (1024, 4096), performance is **on par** or slightly better than the non‑aligned version (155.6 M/s vs. 154.9 M/s).
- The variability (CV) is generally lower for aligned builds, especially at capacity 1024 (0.52% vs. 1.69%), indicating **more stable performance** under contention.

### 3. Why Alignment Might Not Always Win
- On the AMD Ryzen platform, false sharing may not be the dominant bottleneck because the L3 cache is shared and the CPU handles MESI protocol efficiently.
- However, alignment remains a **defensive measure** that prevents unpredictable performance cliffs when the system is under heavy load.

---

*Benchmark #1 ~ #4 executed on 2026‑07‑25.*  
*Benchmark #5 executed on 2026‑07‑26.*  
*All results are reproducible using the provided Google Benchmark suite.*