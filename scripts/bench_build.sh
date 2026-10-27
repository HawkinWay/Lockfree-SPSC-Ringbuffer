#!/usr/bin/env bash
set -euo pipefail

OUT="results/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT"

./scripts/env_snapshot.sh | tee "$OUT/env_snapshot.txt"

cmake -B build-release -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release -j
# ctest --test-dir build-release --output-on-failure


./build-release/benchmarks/ring_buffer_bench \
    --benchmark_filter="Throughput<RB" \
    --benchmark_out="$OUT/throughput.json"  --benchmark_out_format=json \
    2>&1 | tee "$OUT/throughput.txt"

./build-release/benchmarks/ring_buffer_bench \
    --benchmark_filter="Latency" \
    --benchmark_out="$OUT/latency.json"  --benchmark_out_format=json \
    2>&1 | tee "$OUT/latency.txt"