import json
import statistics
import re
import sys
from collections import defaultdict

def load(path):
    """data type: list full of dict"""
    data = json.load(open(path, encoding="utf-8"))["benchmarks"]

    """
    # values = [d["items_per_second"] for d in data]
    values = []
    '''d type: dict'''
    for d in data:
        values.append(d["items_per_second"])
    print("Throughput mean: ", sum(values) / len(values), " ops/s")
    """

    groups = defaultdict(list)
    for d in data:
        if d["run_type"] != "iteration":
            continue
        """eg: d["run_name"] = BM_SPSC_Throughput<RB2<size_t>>/64/repeats:15/real_time"""
        head, *rest = d["run_name"].split("/")
        # head = "BM_SPSC_Throughput<RB2<size_t>>"
        # rest = ["64", "repeats:15", real_time]
        bench, variant = head.split("<", 1)
        # bench = "BM_SPSC_Throughput"
        # variant = "RB2<size_t>>"
        variant = variant.split("<")[0]
        # variant = "RB2"
        nums = tuple(int(p) for p in rest if p.isdigit())
        groups[(bench, variant, *nums)].append(d)   # key: tuple, value: list
    """
    for k, v in groups.items():
        print(k, " -> ", len(v), "items")
    """

    return groups

def med(data, idx):
    return statistics.median([d[idx] for d in data])

"""CV = StandardDeviation / Mean"""
def cv(data, idx):
    vals = [d[idx] for d in data]
    mean = statistics.mean(vals)
    # pstdev -> / n
    # stdev -> / (n - 1)
    return statistics.pstdev(vals) / mean * 100 if mean else 0.0

"""
k = next(iter(groups))
print(k)
print("median:", med(groups[k], "items_per_second"))
print("cv%:", cv(groups[k], "items_per_second"))
"""

def table(title, header, body):
    print("### " + title)
    print()
    print("| " + " | ".join(header) + " |")
    print("|" + "|".join(["---"] * len(header)) + "|")
    for line in body:
        print("| " + " | ".join(line) + " |")
    print()

"""
table(
    "单元素吞吐 (M ops/s, 中位数)",
    ["variant", "cap", "median", "CV%"],
    [
        ["RB2", "64", "23.7", "2.3"],
        ["RB2", "1024", "31.2", "1.1"],
    ],
)
"""

def main(paths):
    groups = defaultdict(list)
    for p in paths:
        for k, v in load(p).items():
            groups[k].extend(v)

    # ---Throughput---
    keys = [k for k in groups if k[0] == "BM_SPSC_Throughput"]
    if keys:
        keys = sorted(keys, key=lambda k: (k[1], k[2]))

        body = []
        for k in keys:
            rows = groups[k]
            body.append([
                k[1],
                str(k[2]),
                format(med(rows, "items_per_second") / 1e6, ".1f"),
                format(cv(rows, "items_per_second"), ".1f")
            ])
        table(
            "Single-Element Throughput (M ops/s, Median)",
            ["Variant", "Capacity", "Median", "CV%"],
            body
        )

    """Batch Throughput"""
    keys = [k for k in groups if k[0] == "BM_SPSC_BatchThroughput"]
    if keys:
        keys = sorted(keys, key=lambda k: (k[1], k[3]))

        body = []
        for k in keys:
            rows = groups[k]
            body.append([
                k[1],
                str(k[3]),
                format(med(rows, "items_per_second") / 1e6, ".1f"),
                format(cv(rows, "items_per_second"), ".1f")
            ])
        table(
            "Batch Throughput (M ops/s, Median) Capacity=4096",
            ["Variant", "Batch Size", "Median", "CV%"],
            body
        )


    """Ping-Pong Latency"""
    keys = [k for k in groups if k[0] == "BM_SPSC_RoundTripLatency"]
    if keys:
        keys = sorted(keys, key=lambda k: (k[1], k[2]))

        body = []
        for k in keys:
            rows = groups[k]
            body.append([
                k[1],
                str(k[2]),
                format(med(rows, "p50_ns"), ".0f"),
                format(med(rows, "p90_ns"), ".0f"),
                format(med(rows, "p95_ns"), ".0f"),
                format(med(rows, "p99_ns"), ".0f"),
                format(med(rows, "max_ns"), ".0f"),
                format(med(rows, "tsc_ns_per_cycle"), ".4f"),
                format(cv(rows, "p50_ns"), ".1f")
            ])
        table(
            "Latency (ns, Median)",
            ["Variant", "Capacity", "p50", "p90", "p95", "p99", "max", "tsc per cycle", "CV%"],
            body
        )


if __name__ == "__main__":
    main(sys.argv[1:])