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