#!/usr/bin/env bash
set -euo pipefail

DIR="results/$(ls -t results | head -1)"

python3 scripts/tool.py \
    "$DIR/throughput.json" \
    "$DIR/latency.json" \
    | tee "$DIR/data.md"