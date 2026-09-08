# /// script
# requires-python = ">=3.11"
# dependencies = ["matplotlib>=3.10,<4"]
# ///

"""Plot the generated single-allocation SoA benchmark's CSV records."""
from __future__ import annotations

import argparse
import csv
import importlib
import io
import os
import tempfile
import math
import re
import sys
from pathlib import Path
from typing import Any, cast

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_INPUT = ROOT / "out/build/benchmark/Testing/Temporary/LastTest.log"
DEFAULT_OUTPUT = ROOT / ".local/benchmarks/single-allocation/plots"
COLORS = ("#4477AA", "#EE7733")
LABELS = {
    "natural_append_1": "Natural append (1 row)",
    "natural_append_64": "Natural append (64 rows)",
    "reserved_append_1": "Reserved append (1 row)",
    "reserved_append_64": "Reserved append (64 rows)",
    "defaulted_append_1": "Reserved defaulted append (1 row)",
    "defaulted_append_64": "Reserved defaulted append (64 rows)",
    "reserve": "Reserve empty owner",
    "populated_growth": "Grow populated owner",
    "set_num_grow": "Reserved set_num grow",
    "set_num_shrink_reuse": "Shrink/refill (4,096 cycles)",
    "reset_reuse": "Reset/refill (4,096 cycles)",
    "remove_swap": "Swap-remove quarter of rows",
    "iterate": "Narrow iteration (4 passes)",
    "iterate_wide": "Wide iteration (4 passes)",
    "construct_views": "Construct/consume views (4,096 calls)",
}
TIMING_FIELDS = [
    "count", "operation", "baseline_median_ns", "baseline_p10_ns", "baseline_p90_ns",
    "single_median_ns", "single_p10_ns", "single_p90_ns", "baseline_over_single",
]
ALLOCATION_FIELDS = [
    "owner", "count", "reserved", "allocation_requests", "retained_blocks",
    "requested_bytes", "usable_bytes", "live_bytes", "row_slack_bytes", "padding_bytes",
    "peak_requested_bound", "min_capacity", "max_capacity", "owner_bytes",
]
Record = dict[str, str]


def read_results(paths: list[Path]) -> tuple[list[Record], list[Record]]:
    groups: dict[str, dict[tuple[str, ...], Record]] = {"SOA_TIMING": {}, "SOA_ALLOCATION": {}}
    for path in paths:
        csv_kind = ""
        for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
            match = re.match(r"^\s*(?:\d+:\s*)?(SOA_TIMING|SOA_ALLOCATION),(.*)$", line)
            if match:
                kind, payload = match.groups()
            elif line.startswith("count,operation,"):
                csv_kind = "SOA_TIMING"
                continue
            elif line.startswith("owner,count,reserved,"):
                csv_kind = "SOA_ALLOCATION"
                continue
            elif csv_kind and line.strip():
                kind, payload = csv_kind, line
            else:
                continue
            fields = TIMING_FIELDS if kind == "SOA_TIMING" else ALLOCATION_FIELDS
            values = next(csv.reader([payload]))
            if values == fields:
                continue
            context = f"{path}:{number}"
            if len(values) != len(fields):
                raise ValueError(f"{context}: expected {len(fields)} CSV fields, got {len(values)}")
            row = dict(zip(fields, values, strict=True))
            for field in fields:
                if field in {"owner", "operation"}:
                    continue
                value = float(row[field])
                if not math.isfinite(value) or value < 0:
                    raise ValueError(f"{context}: invalid {field}: {value}")
            if int(row["count"]) <= 0:
                raise ValueError(f"{context}: count must be positive")
            if kind == "SOA_TIMING":
                for owner in ("baseline", "single"):
                    low, median, high = (float(row[f"{owner}_{stat}_ns"]) for stat in ("p10", "median", "p90"))
                    if not 0 < low <= median <= high:
                        raise ValueError(f"{context}: invalid timing percentiles for {owner}")
                key = (row["count"], row["operation"])
            else:
                if row["owner"] not in {"TArray", "Single"} or row["reserved"] not in {"0", "1"}:
                    raise ValueError(f"{context}: unknown owner or reserve mode")
                key = (row["count"], row["reserved"], row["owner"])
            previous = groups[kind].get(key)
            if previous is not None and previous != row:
                raise ValueError(f"{context}: conflicting results for {key}; use one benchmark run")
            groups[kind][key] = row
    timings = list(groups["SOA_TIMING"].values())
    allocations = list(groups["SOA_ALLOCATION"].values())
    if not timings:
        raise ValueError("No SOA_TIMING records found. Run the full benchmark workflow or pass --input with a saved timing log/CSV.")
    return timings, allocations


def save_figure(plt: Any, figure: Any, output: Path, name: str) -> None:
    for extension in ("png", "svg"):
        destination = output / f"{name}.{extension}"
        image = io.BytesIO()
        figure.savefig(image, format=extension, dpi=160, bbox_inches="tight")
        # Replace a complete image instead of truncating a file held by an image viewer.
        with tempfile.NamedTemporaryFile(dir=output, prefix=f"{name}-", suffix=f".{extension}", delete=False) as temporary:
            temporary.write(image.getvalue())
            temporary_path = Path(temporary.name)
        try:
            os.replace(temporary_path, destination)
        except OSError as error:
            print(f"Could not replace {destination} ({error}); saved a new image instead.", file=sys.stderr)
            destination = temporary_path
        print(destination)
    plt.close(figure)


def plot_timings(plt: Any, rows: list[Record], output: Path) -> None:
    operations = list(dict.fromkeys(row["operation"] for row in rows))
    figure, axes = plt.subplots(math.ceil(len(operations) / 3), 3, figsize=(16, 3.3 * math.ceil(len(operations) / 3)), squeeze=False)
    for axis, operation in zip(axes.flat, operations):
        points = sorted((row for row in rows if row["operation"] == operation), key=lambda row: int(row["count"]))
        counts = [int(row["count"]) for row in points]
        for owner, label, color in zip(("baseline", "single"), ("TArray", "Single allocation"), COLORS):
            median = [float(row[f"{owner}_median_ns"]) / 1e6 for row in points]
            lower = [m - float(row[f"{owner}_p10_ns"]) / 1e6 for m, row in zip(median, points)]
            upper = [float(row[f"{owner}_p90_ns"]) / 1e6 - m for m, row in zip(median, points)]
            axis.errorbar(counts, median, yerr=[lower, upper], marker="o", capsize=3, label=label, color=color)
        axis.set(title=LABELS.get(operation, operation), xlabel="Rows", ylabel="Milliseconds (log scale)", xscale="log", yscale="log")
        axis.set_xticks(counts, [f"{count:,}" for count in counts])
        axis.grid(alpha=0.2)
    for axis in list(axes.flat)[len(operations):]:
        axis.set_visible(False)
    axes.flat[0].legend()
    figure.suptitle("SoA operation times — medians with p10–p90 sample variability", fontsize=16)
    figure.text(0.5, 0.005, "Reserved cases exclude reserve cost. Uninitialised append does not write row contents. Percentiles are not confidence intervals.", ha="center")
    figure.tight_layout(rect=(0, 0.03, 1, 0.97))
    save_figure(plt, figure, output, "timings")


def plot_speedups(plt: Any, rows: list[Record], output: Path) -> None:
    operations = list(dict.fromkeys(row["operation"] for row in rows))
    counts = sorted({int(row["count"]) for row in rows})
    lookup = {(row["operation"], int(row["count"])): float(row["baseline_median_ns"]) / float(row["single_median_ns"]) for row in rows}
    values = [[math.log2(lookup[(op, count)]) if (op, count) in lookup else math.nan for count in counts] for op in operations]
    limit = max(1.0, max(abs(value) for row in values for value in row if math.isfinite(value)))
    figure, axis = plt.subplots(figsize=(10, max(4, len(operations) * 0.43)))
    chart = axis.imshow(values, cmap="RdBu", vmin=-limit, vmax=limit, aspect="auto")
    for y, operation in enumerate(operations):
        for x, count in enumerate(counts):
            ratio = lookup.get((operation, count))
            axis.text(x, y, "—" if ratio is None else f"{ratio:.2f}×", ha="center", va="center", color="white" if ratio and abs(math.log2(ratio)) > limit * 0.65 else "black")
    axis.set_xticks(range(len(counts)), [f"{count:,} rows" for count in counts])
    axis.set_yticks(range(len(operations)), [LABELS.get(op, op) for op in operations])
    axis.set_title("TArray time / single-allocation time\nAbove 1×: single allocation faster; below 1×: slower")
    figure.colorbar(chart, ax=axis, label="log₂(time ratio), centred at equal performance")
    figure.tight_layout()
    save_figure(plt, figure, output, "relative-performance")


def plot_allocations(plt: Any, rows: list[Record], output: Path) -> None:
    metrics = [("requested_bytes", "Requested storage", 2**20, "MiB"), ("usable_bytes", "Allocator-usable storage", 2**20, "MiB"), ("row_slack_bytes", "Unused row capacity", 2**20, "MiB"), ("peak_requested_bound", "Peak requested storage bound", 2**20, "MiB"), ("allocation_requests", "Capacity-changing requests", 1, "Requests"), ("retained_blocks", "Retained allocations", 1, "Blocks")]
    keys = sorted({(int(row["count"]), row["reserved"]) for row in rows})
    lookup = {(int(row["count"]), row["reserved"], row["owner"]): row for row in rows}
    figure, axes = plt.subplots(3, 2, figsize=(15, 12))
    for axis, (field, title, divisor, unit) in zip(axes.flat, metrics):
        for owner_index, (owner, color) in enumerate(zip(("TArray", "Single"), COLORS)):
            values = [float(lookup[(*key, owner)][field]) / divisor if (*key, owner) in lookup else math.nan for key in keys]
            axis.bar([index + (owner_index - 0.5) * 0.36 for index in range(len(keys))], values, width=0.36, label=owner, color=color)
        axis.set(title=title, ylabel=unit)
        axis.set_xticks(range(len(keys)), [f"{count:,}\n{'Reserved' if reserved == '1' else 'Natural'}" for count, reserved in keys], rotation=25)
        axis.grid(axis="y", alpha=0.2)
    axes.flat[0].legend()
    figure.suptitle("Allocation diagnostics — same logical rows and columns", fontsize=16)
    figure.text(0.5, 0.005, "Requests count capacity transitions, not physical malloc calls. Peak is a requested-byte bound, not measured RSS. Missing data is omitted.", ha="center")
    figure.tight_layout(rect=(0, 0.03, 1, 0.97))
    save_figure(plt, figure, output, "allocations")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, nargs="+", default=[DEFAULT_INPUT], help="CTest logs, captured console output, or exported CSV files (default: latest benchmark CTest log)")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    try:
        timings, allocations = read_results(args.input)
        matplotlib = cast(Any, importlib.import_module("matplotlib"))
        matplotlib.use("Agg")
        plt = cast(Any, importlib.import_module("matplotlib.pyplot"))
        args.output_dir.mkdir(parents=True, exist_ok=True)
        for name, fields, rows in (("timings", TIMING_FIELDS, timings), ("allocations", ALLOCATION_FIELDS, allocations)):
            with (args.output_dir / f"{name}.csv").open("w", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=fields)
                writer.writeheader()
                writer.writerows(rows)
        plot_timings(plt, timings, args.output_dir)
        plot_speedups(plt, timings, args.output_dir)
        if allocations:
            plot_allocations(plt, allocations, args.output_dir)
        else:
            print("No allocation records supplied; generated timing plots only.", file=sys.stderr)
        return 0
    except (OSError, ValueError, ImportError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
