# /// script
# requires-python = ">=3.11"
# dependencies = ["matplotlib>=3.10,<4"]
# ///
"""Plot standalone SoA Google Benchmark JSON as PNG images."""
from __future__ import annotations

import argparse
import importlib
import json
import math
from pathlib import Path
from typing import Any, cast


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    document = json.loads(args.input.read_text(encoding="utf-8"))
    rows: dict[tuple[str, str, int], list[float]] = {}
    for record in document["benchmarks"]:
        if record.get("error_occurred"):
            raise ValueError(record.get("error_message", "Benchmark failed"))
        if record.get("run_type") == "aggregate":
            continue
        owner, operation, count, *_ = record["run_name"].split("/")
        scale = {"ns": 1e-6, "us": 1e-3, "ms": 1, "s": 1e3}[record["time_unit"]]
        value = float(record["real_time"]) * scale
        if not math.isfinite(value) or value <= 0:
            raise ValueError("Non-positive or non-finite timing")
        rows.setdefault((operation, owner, int(count)), []).append(value)
    if not rows:
        raise ValueError("No benchmark repetitions found")
    matplotlib = cast(Any, importlib.import_module("matplotlib"))
    matplotlib.use("Agg")
    plt = cast(Any, importlib.import_module("matplotlib.pyplot"))
    operations = sorted({key[0] for key in rows})
    figure, axes = plt.subplots(len(operations), 1, figsize=(10, 4 * len(operations)), squeeze=False)
    for axis, operation in zip(axes.flat, operations):
        for owner in ("Vector", "Single"):
            counts = sorted(key[2] for key in rows if key[:2] == (operation, owner))
            if not counts:
                continue
            samples = [rows[(operation, owner, count)] for count in counts]
            means = [sum(values) / len(values) for values in samples]
            lower = [mean - min(values) for mean, values in zip(means, samples)]
            upper = [max(values) - mean for mean, values in zip(means, samples)]
            axis.errorbar(counts, means, yerr=[lower, upper], marker="o", capsize=3, label=owner)
        axis.set(title=operation, xlabel="Rows", ylabel="Milliseconds", xscale="log", yscale="log")
        axis.grid(alpha=.2)
        axis.legend()
    figure.suptitle("Native SoA: mean and min–max across repetitions (not confidence intervals)")
    figure.tight_layout()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    # Reuse the existing atomic image writer, including its locked-viewer fallback.
    helpers = cast(Any, importlib.import_module("plot-single-allocation-soa-benchmarks"))
    helpers.save_figure(plt, figure, args.output_dir, "native-soa-timings")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
