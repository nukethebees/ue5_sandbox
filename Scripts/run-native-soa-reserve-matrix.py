# /// script
# requires-python = ">=3.11"
# dependencies = ["matplotlib>=3.10,<4"]
# ///
"""Run each native reserve matrix entry in a fresh process, then export JSON, CSV and PNG."""
from __future__ import annotations

import argparse
import csv
import importlib
import json
import math
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any, cast

ROOT = Path(__file__).resolve().parent.parent


def run_with_benchmark_access() -> int | None:
    if (
        "--dry-run" in sys.argv
        or os.environ.get("SANDBOX_MACHINE_ACTIVITY_MODE") == "benchmark"
    ):
        return None

    module = ROOT / "cmake" / "machine_activity.cmake"
    runner = ROOT / "cmake" / "run_with_machine_activity.cmake"
    command = [
        "cmake",
        f"-DMACHINE_ACTIVITY_MODULE={module}",
        "-DMACHINE_ACTIVITY_MODE=benchmark",
        "-DMACHINE_ACTIVITY_OPERATION=native SoA reserve matrix",
        "-P",
        str(runner),
        "--",
        sys.executable,
        str(Path(__file__).resolve()),
        *sys.argv[1:],
    ]
    return subprocess.run(command, cwd=ROOT, check=False).returncode

OWNERS = (1, 2, 4, 8, 16, 32, 64, 128, 200, 256, 512)
IMPLEMENTATIONS = ("Vector", "Single", "RawMalloc", "RawRealloc")


def plot(document: dict[str, Any], output: Path) -> None:
    samples: dict[tuple[str, int], list[float]] = {}
    for run in document["runs"]:
        label = f"{run['backend']} {run['implementation']}"
        values: list[float] = []
        for record in run["result"]["benchmarks"]:
            if record.get("error_occurred"):
                raise ValueError(record.get("error_message", "Benchmark failed"))
            if record.get("run_type") == "aggregate":
                continue
            scale = {"ns": 1e-6, "us": 1e-3, "ms": 1, "s": 1e3}[record["time_unit"]]
            value = float(record["real_time"]) * scale
            if not math.isfinite(value) or value <= 0:
                raise ValueError("Invalid benchmark timing")
            values.append(value)
        if not values:
            raise ValueError(f"No measurements for {label}")
        samples[label, run["owners"]] = values
    labels = list(dict.fromkeys(label for label, _ in samples))
    owners = document["owners"]
    matplotlib = cast(Any, importlib.import_module("matplotlib"))
    matplotlib.use("Agg")
    plt = cast(Any, importlib.import_module("matplotlib.pyplot"))
    figure, axes = plt.subplots(1, 2, figsize=(16, 6))
    for label in labels:
        for axis, amortized in zip(axes, (False, True)):
            scaled = [[v / (count if amortized else 1) for v in samples[label, count]] for count in owners]
            means = [sum(v) / len(v) for v in scaled]
            errors = [[mean - min(v) for mean, v in zip(means, scaled)], [max(v) - mean for mean, v in zip(means, scaled)]]
            axis.errorbar(owners, means, yerr=errors, marker="o", capsize=2, label=label)
            axis.set(xscale="log", yscale="log", xlabel="Simultaneously retained owners / blocks", ylabel="Milliseconds per owner" if amortized else "Milliseconds per batch")
            axis.grid(alpha=0.2)
    axes[0].legend(fontsize=8)
    prefix = "DRY RUN — " if document["dry_run"] else ""
    figure.suptitle(f"{prefix}Native reserve: {document['rows']:,} rows — fresh process per entry, includes cleanup\nMean and min–max across repetitions (not confidence intervals)")
    figure.tight_layout()
    helpers = cast(Any, importlib.import_module("plot-single-allocation-soa-benchmarks"))
    helpers.save_figure(plt, figure, output, "native-reserve-matrix")
    with (output / "native-reserve-matrix.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(["owners", *labels])
        for count in owners:
            writer.writerow([count, *[sum(samples[label, count]) / len(samples[label, count]) for label in labels]])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--standard", type=Path)
    parser.add_argument("--mimalloc", type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--rows", type=int, choices=(4096, 65536, 1048576), default=65536)
    parser.add_argument("--owners", type=int, nargs="+", choices=OWNERS, default=list(OWNERS))
    parser.add_argument("--repetitions", type=int, default=10)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--plot-only", action="store_true")
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    combined = args.output_dir / "native-reserve-matrix.json"
    if args.plot_only:
        plot(json.loads(combined.read_text(encoding="utf-8")), args.output_dir)
        return
    if not args.standard or not args.mimalloc or args.repetitions < 1:
        parser.error("Both executables and a positive repetition count are required")
    document: dict[str, Any] = {"rows": args.rows, "owners": args.owners, "dry_run": args.dry_run, "runs": []}
    for owners in args.owners:
        for backend, executable in (("Standard", args.standard), ("Mimalloc", args.mimalloc)):
            for implementation in IMPLEMENTATIONS:
                name = f"{implementation}/reserve_batch/{args.rows}/{owners}/real_time"
                stem = f"{backend}-{implementation}-{args.rows}-{owners}"
                destination = args.output_dir / f"{stem}.json"
                command = [str(executable.resolve()), f"--benchmark_filter=^{re.escape(name)}$", f"--benchmark_repetitions={args.repetitions}", "--benchmark_min_time=0.1s", f"--benchmark_out={destination.resolve()}", "--benchmark_out_format=json"]
                if args.dry_run:
                    command.append("--benchmark_dry_run")
                print(f"Running {backend} {name}", flush=True)
                with (args.output_dir / f"{stem}.log").open("w", encoding="utf-8") as log:
                    subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
                result = json.loads(destination.read_text(encoding="utf-8"))
                if {record.get("run_name") for record in result["benchmarks"]} != {name}:
                    raise ValueError(f"Expected exactly one benchmark: {name}")
                document["runs"].append({"backend": backend, "implementation": implementation, "owners": owners, "result": result})
                combined.write_text(json.dumps(document, indent=2), encoding="utf-8")
    plot(document, args.output_dir)


if __name__ == "__main__":
    activity_result = run_with_benchmark_access()
    if activity_result is not None:
        raise SystemExit(activity_result)
    main()
