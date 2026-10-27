#!/usr/bin/env bash
set -u

{
  date -Is
  cat /sys/devices/system/clocksource/clocksource0/current_clocksource 2>/dev/null || echo '-'
  cat /sys/devices/system/clocksource/clocksource0/available_clocksource 2>/dev/null || echo '-'
  grep -m1 '^flags' /proc/cpuinfo | tr ' ' '\n' | grep -E 'constant_tsc|nonstop_tsc' || true
  lscpu | grep -E 'Model name|CPU\(s\)|Core|Socket|MHz|Hypervisor' || true
  cat /sys/devices/system/cpu/cpu0/topology/thread_siblings_list 2>/dev/null || echo '-'
  cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null
  uptime
}