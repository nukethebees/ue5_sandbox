# /// script
# requires-python = ">=3.11"
# dependencies = ["matplotlib>=3.10,<4"]
# ///
"""Plot a native reserve matrix produced by BenchmarkTools."""
from __future__ import annotations

import argparse
import importlib
import json
import math
from pathlib import Path
from typing import Any, cast


def timing_values(result: dict[str, Any]) -> list[float]:
    values: list[float] = []
    for record in result["benchmarks"]:
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
        raise ValueError("No benchmark measurements")
    return values


def plot(document: dict[str, Any], output: Path) -> None:
    samples: dict[tuple[str, int], list[float]] = {}
    for run in document["runs"]:
        samples[(f"{run['backend']} {run['implementation']}", run["owners"])] = timing_values(run["result"])
    matplotlib = cast(Any, importlib.import_module("matplotlib"))
    matplotlib.use("Agg")
    plt = cast(Any, importlib.import_module("matplotlib.pyplot"))
    labels = list(dict.fromkeys(label for label, _ in samples))
    owners = document["owners"]
    figure, axes = plt.subplots(1, 2, figsize=(16, 6))
    for label in labels:
        for axis, amortized in zip(axes, (False, True)):
            values = [[value / (count if amortized else 1) for value in samples[label, count]] for count in owners]
            means = [sum(group) / len(group) for group in values]
            errors = [[mean - min(group) for mean, group in zip(means, values)], [max(group) - mean for mean, group in zip(means, values)]]
            axis.errorbar(owners, means, yerr=errors, marker="o", capsize=2, label=label)
            axis.set(xscale="log", yscale="log", xlabel="Simultaneously retained owners / blocks", ylabel="Milliseconds per owner" if amortized else "Milliseconds per batch")
            axis.grid(alpha=0.2)
    axes[0].legend(fontsize=8)
    prefix = "DRY RUN — " if document["dryRun"] else ""
    figure.suptitle(f"{prefix}Native reserve: {document['rows']:,} rows — fresh process per entry, includes cleanup\nMean and min–max across repetitions (not confidence intervals)")
    figure.tight_layout()
    output.mkdir(parents=True, exist_ok=True)
    for extension in ("png", "svg"):
        figure.savefig(output / f"native-reserve-matrix.{extension}", dpi=160, bbox_inches="tight")
    plt.close(figure)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    try:
        plot(json.loads(args.input.read_text(encoding="utf-8")), args.output_dir)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"error: {error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
