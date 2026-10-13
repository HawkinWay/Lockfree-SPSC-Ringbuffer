# SPSC Ring Buffer Benchmark Report

## Summary

This benchmark compares 6 SPSC ring-buffer implementations: 

1. baseline
2. default `seq_cst`
3. `acquire`/`release` atomics
4. cache-line padding
5. power-of-two index wrapping
6. batch APIs. 

The clearest result is that **`acquire`/`release` substantially outperformed `seq_cst` in this run**. Batch operations increased reported throughput as batch size grew, reaching **2.515 G/s at batch size 256**. Alignment and bitwise wrapping did not show a consistent standalone gain.

## Setup and measurement notes

- **Test Platform:** AMD Ryzen 7 (8 cores / 16 threads, 5.13 GHz, Ubuntu Linux)  
- **Compiler:** GCC 11.4 (`-O3` - Release)  
- **Benchmark Tool:** Google Benchmark (`Repetitions(15)`, `ReportAggregatesOnly`, `UseRealTime`); all variants were run in one binary/process, with 15 repetitions per configuration. **Tables report medians.**
- **CPU frequency note:** 3319.08 MHz (from the run log reports)
- **Counter units:** the benchmark calls `SetItemsProcessed(... * 2)`, counting a push and a pop for each transferred element. “Reported throughput” uses that counter; it is not a count of batch API calls. The reciprocal column is a derived rate, **not measured operation latency**. For batch results, the full element-transfer rate is half the reported rate.
- **Noise:** single-element results are sensitive to code layout; absolute rates shifted by up to ±40% across differently built binaries[(see session-cross_tests/)](benchmark_records/session-cross_tests/). Across sessions, most configurations moved by 5–15%, and `seq_cst` by as much as +37%. The original report observed batch results reproducing within ~2% across builds, although short-run batch CVs reached 8.38%. Small cross-session differences should not be treated as reliable wins.

## Single-element results

Throughput and reciprocal rate are the original report's median values. CV is the reported real-time coefficient of variation.

| Variant | Capacity | Reported throughput (median)| Reciprocal rate (median) | CV |
|:---|:---:|:---:|:---:|---:|
| RingBuffer1 — plain indices, **data races / UB** | 64 | 220.95 M/s | 4.53 ns | 1.58% |
| | 1024 | 297.32 M/s | 3.36 ns | 0.66% |
|  | 4096 | 289.20 M/s | 3.46 ns | 0.41% |
| RingBuffer2 — `seq_cst` | 64 | 91.59 M/s | 10.92 ns | 1.71% |
|  | 1024 | 72.81 M/s | 13.74 ns | 3.03% |
|  | 4096 | 72.96 M/s | 13.71 ns | 2.37% |
| RingBuffer3 — `acquire`/`release` | 64 | 231.11 M/s | 4.33 ns | 6.03% |
|  | 1024 | 289.88 M/s | 3.45 ns | 0.99% |
|  | 4096 | 283.72 M/s | 3.52 ns | 0.56% |
| RingBuffer4 — cache-line aligned | 64 | 178.78 M/s | 5.59 ns | 0.51% |
|  | 1024 | 305.25 M/s | 3.28 ns | 1.14% |
|  | 4096 | 299.10 M/s | 3.34 ns | 1.09% |
| RingBuffer5 — power-of-two bitwise wrapping | 64 | 184.54 M/s | 5.42 ns | 0.78% |
|  | 1024 | 301.78 M/s | 3.31 ns | 1.30% |
|  | 4096 | 298.67 M/s | 3.35 ns | 1.18% |
| RingBuffer6 — std::memcpy for trivially copyable types | 64 | 169.50 M/s | 5.90 ns | 0.48% |
|  | 1024 | 352.45 M/s | 2.84 ns | 1.41% |
|  | 4096 | 335.76 M/s | 2.98 ns | 0.52% |

## Batch results — RingBuffer6

*capacity 4096*  
`push_batch` / `pop_batch` added

| Batch size | Reported throughput (median)| Reciprocal rate (median) | CV |
|---|:---:|:---:|---|
| 8 | 763.47 M/s | 1.31 ns | 3.28% |
| 32 | 1.8317 G/s | 0.55 ns | 6.06% |
| 64 | 2.1329 G/s | 0.47 ns | 8.38% |
| 256 | **2.5151 G/s** | 0.40 ns | 4.12% |

At batch size 256, the reported rate corresponds to about **1.258 billion complete element transfers per second** when a push-plus-pop pair is counted once. The 0.40 ns figure is the reciprocal of the double-counted rate, not a measured end-to-end element latency. The original report also described this as roughly 9× the RingBuffer1 result at capacity 4096; that comparison is informal because RingBuffer1 has undefined behavior.

## What the results suggest

1. **Memory ordering had the largest measured effect.** In this run, acquire/release reached 2.5× the `seq_cst` throughput at capacity 64, 4.0× at 1024, and 3.9× at 4096. Earlier setups reported ratios of 4.2×, 5.7×, and 4.0×.
2. **Cache-line alignment had no consistent benefit.** Compared with RingBuffer3, RingBuffer4 was about 23% slower at capacity 64 and 5.3% / 5.4% faster at 1024 / 4096 in this run. Earlier setups ranged from approximately 0% to +14% and −12%, so a repeatable general speedup was not established.
3. **Bitwise wrapping changes the generated code, but its throughput gain was not clear.** Replacing `% capacity` with `& (capacity - 1)` removes runtime modulo for power-of-two capacities; use disassembly can confirm the change. In this run, RingBuffer5 differed from RingBuffer4 by +3.2% at capacity 64, −1.1% at 1024, and −0.1% at 4096.
4. **Batching increased throughput, with diminishing gains at larger batches.** Relative to RingBuffer6's single-element result at capacity 4096, batch sizes 8, 32, 64, and 256 reached 2.3×, 5.5×, 6.4×, and 7.5× the reported rate. Batch 256 was about 34.5× the `seq_cst` single-element result at capacity 4096. Moving from batch 8→32, 32→64, and 64→256 improved reported throughput by about 2.4×, 1.16×, and 1.18×, respectively; the apparent plateau varied between setups (around batch 32 or 64). Profiling would be needed to identify the limiting factor.
5. **Treat the unsynchronized baseline only as a reference.** RingBuffer1 has data races and undefined behavior, so its results are not a valid performance ceiling. RingBuffer6 was about 16.8% faster than RingBuffer5 at capacity 1024 and 12.4% faster at 4096 in this run, but across three setups the reported difference ranged from −8% to about 0% to +17%. Their single-element paths compiled to equivalent machine code: for `size_t`, the 8-byte `memcpy` was inlined to a single `mov` in the inspected disassembly. This suggests code-layout sensitivity rather than a dependable implementation win, and does not establish the same cost for every type.

## Conclusion

The most convincing result is the large within-run improvement from `seq_cst` to acquire/release ordering. Batch APIs also show a strong throughput advantage for this workload, while the isolated benefits of cache-line padding and bitwise wrapping remain inconclusive. The power-of-two capacity requirement enables the batch path, and `memcpy` provides an efficient route for trivially copyable data.

These measurements do **not** by themselves establish that the implementation is production-ready or real-time safe. That requires correctness coverage (including wrap-around, full/empty behavior, ordering, and long-running concurrent tests) in addition to performance results. The unsynchronized variant must not be used as a correct SPSC implementation.
