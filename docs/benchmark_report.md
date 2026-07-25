# Benchmark Report

---

## v0.1 Foundation

### 📈 SPSC RingBuffer Baseline (Issue #1)

* **Test Platform**: Apple M4 (10 Cores, macOS)
* **Compiler**: Apple Clang (`-O3` Release)
* **Memory Order**: None
* **Alignment**: None

| Capacity | Operation Latency(avg) | Throughput(ops/s) | 5-run std. dev. (CV) |
|:---------|:-----------------------|:------------------|:------|
| **64**   | ~3.63 ns               | **275.04 M/s**    | 1.11% |
| **1024** | ~3.23 ns               | **308.70 M/s**    | 0.51% |
| **4096** | ~3.18 ns               | **313.75 M/s**    | 0.84% |

> ⚠️ This implementation is intentionally unsafe and only used as a performance reference. 
> It contains data races under concurrent access.

### 📈 SPSC RingBuffer Atomic SeqCst (Issue #2)

* **Test Platform**: Apple M4 (10 Cores, macOS)
* **Compiler**: Apple Clang (`-O3` Release)
* **Memory Order**: Default `std::memory_order_seq_cst`
* **Alignment**: Default Non-alignment (Maybe have false sharing)

| Capacity | Operation Latency(avg) | Throughput(ops/s) | 5-run std. dev. (CV) |
|:---------|:-----------------------|:------------------|:---------------------|
| **64**   | ~18.41 ns              | **54.31 M/s**     | 1.73%                |
| **1024** | ~17.62 ns              | **56.74 M/s**     | 1.09%                |
| **4096** | ~16.58 ns              | **60.32 M/s**     | 1.99%                |


### 👀 Observation

Replacing plain indices with `std::atomic` using default
`memory_order_seq_cst` introduces significant synchronization overhead.

Performance drops from:

*313.75M ops/s* to *60.32M ops/s* (~80% reduction)

This provides the motivation for exploring acquire/release ordering and cache-line optimization.

---

## v0.2 Optimization

### 📈 SPSC RingBuffer Acquire-Release (Issue #3)

* **Test Platform**: Apple M4 (10 Cores, macOS)
* **Compiler**: Apple Clang (`-O3` Release)
* **Memory Order**: `std::memory_order_relaxed`, `std::memory_order_release`, `std::memory_order_acquire`
* **Alignment**: Default Non-alignment (Maybe have false sharing)

| Capacity | Operation Latency(avg) | Throughput(ops/s) | 5-run std. dev. (CV) |
|:---------|:-----------------------|:------------------|:---------------------|
| **64** | ~15.46 ns              | **64.65 M/s** | 6.98%                |
| **1024** | ~14.94 ns              | **66.95 M/s** | 9.37%                |
| **4096** | ~16.52 ns              | **60.53 M/s** | 11.86%               |

### 👀 Observation

Switching from `seq_cst` to fine-grained Acquire-Release memory ordering yields only a minor performance gain (from **60.32M ops/s** to **66.95M ops/s**, ~10% improvement).

This key observation reveals that instruction reordering and full memory barriers are **not the primary bottleneck** at this stage. Instead, the system is severely memory-bound due to **False Sharing**: `write_idx` and `read_idx` reside on the same 64-byte cache line, causing constant cache-line invalidation (cache ping-pong) between CPU cores via the MESI protocol.