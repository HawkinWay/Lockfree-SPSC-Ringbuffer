#!/usr/bin/env bash
set -u

COMMIT="$(git rev-parse --short HEAD 2>/dev/null || echo '-')"
if ! git diff --quiet 2>/dev/null; then COMMIT="${COMMIT}-dirty"; fi

{
  echo "date: $(date +%Y-%m-%dT%H:%M:%S%z)"
  echo "commit: $COMMIT"
  echo "kernel: $(uname -r)"
  echo "clocksource: $(cat /sys/devices/system/clocksource/clocksource0/current_clocksource 2>/dev/null || echo '-')"
  echo "available: $(cat /sys/devices/system/clocksource/clocksource0/available_clocksource 2>/dev/null || echo '-')"
  echo "tsc flags:"
  grep -m1 '^flags' /proc/cpuinfo 2>/dev/null | tr ' ' '\n' | grep -E 'constant_tsc|nonstop_tsc' || true
  echo "cpu:"
  lscpu 2>/dev/null | grep -E 'Model name|^CPU\(s\)|Core\(s\) per socket|Socket|MHz|Hypervisor' || true
  echo "siblings0: $(cat /sys/devices/system/cpu/cpu0/topology/thread_siblings_list 2>/dev/null || echo '-')"
  echo "governor: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo '-')"
  echo "boost: $(cat /sys/devices/system/cpu/cpufreq/boost 2>/dev/null || echo '-')"
  echo "producer_cpu: ${SPSC_PRODUCER_CPU:-0}"
  echo "consumer_cpu: ${SPSC_CONSUMER_CPU:-2}"
  echo "nproc: $(nproc 2>/dev/null || echo '?')"
  uptime
}
