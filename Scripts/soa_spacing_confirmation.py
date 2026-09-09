"""Case selection and reporting for the pinned-CPU SoA spacing confirmation."""
from __future__ import annotations

import csv
import importlib
import json
import statistics
from pathlib import Path
from typing import Any, cast

Record = dict[str, Any]
SCHEMAS = ("EntityData", "SpacingDoubles", "SpacingMixedWidths", "SpacingAligned")
CAPACITIES = (65536, 65600, 75008, 100032, 131072)
GAPS = (0, 64, 192)


def confirmation_cases(smoke: bool = False, *, generated_gap: int = 192) -> list[Record]:
    result: list[Record] = []
    for schema in range(4):
        capacities = (65536,) if smoke else CAPACITIES if schema == 0 else (65536, 75008, 131072)
        for capacity in capacities:
            for gap in GAPS:
                for wide in (0, 1):
                    result.append(dict(capacity=capacity, gap=gap, owner=0, wide=wide, live=65535, schema=schema))
    for capacity in ((65536,) if smoke else CAPACITIES):
        for wide in (0, 1):
            result.append(dict(capacity=capacity, gap=generated_gap, owner=1, wide=wide, live=65535, schema=0))
    for wide in (0, 1):
        result.append(dict(capacity=65536, gap=0, owner=2, wide=wide, live=65535, schema=0))
    return result


def field_paths(root: Path, schema: int) -> list[str]:
    # Read names from the same manifests as codegen; ordinal order is checked against diagnostics.
    structs: dict[str, Record] = {}
    for name in ("single_allocation_experiment.json", "soa_spacing_shapes.json"):
        document = json.loads((root / "Codegen/manifests" / name).read_text(encoding="utf-8"))
        for module in document["modules"]:
            structs.update({item["name"]: item for item in module["structs"]})
    def flatten(name: str, prefix: str = "") -> list[str]:
        paths: list[str] = []
        for member in structs[name]["members"]:
            path = prefix + member["name"]
            if member["kind"] == "nested":
                paths += flatten(member["nested_schema"], path + ".")
            else:
                paths.append(path)
        return paths
    return flatten(SCHEMAS[schema])


def summarize(records: list[Record]) -> tuple[list[Record], Record]:
    groups: dict[tuple[int, int, int, int, int], list[Record]] = {}
    for row in records:
        key = (row["schema"], row["capacity"], row["wide"], row["owner"], row["gap"])
        groups.setdefault(key, []).append(row)
    rows: list[Record] = []
    for (schema, capacity, wide, owner, gap), points in sorted(groups.items()):
        rows.append(dict(schema=SCHEMAS[schema], schema_id=schema, capacity=capacity, wide=wide, owner=owner, gap=gap,
                         processes=len(points), median_ns=statistics.median(point["mean_ns"] for point in points),
                         minimum_ns=min(point["mean_ns"] for point in points), maximum_ns=max(point["mean_ns"] for point in points),
                         extra_bytes=points[0]["extra_bytes"], layout_identity=points[0]["layout_identity"]))
    candidates: list[Record] = []
    equivalent_layouts: list[Record] = []
    for row in rows:
        if row["owner"] != 0:
            continue
        peers = [peer for peer in rows if peer["owner"] == 0 and peer["schema"] == row["schema"] and
                 peer["capacity"] == row["capacity"] and peer["wide"] == row["wide"] and peer["layout_identity"] == row["layout_identity"]]
        equivalent_gaps = sorted(peer["gap"] for peer in peers)
        if len(peers) > 1 and row["gap"] == equivalent_gaps[0]:
            equivalent_layouts.append(dict(schema=row["schema"], capacity=row["capacity"], wide=row["wide"], gaps=equivalent_gaps))
        means = [point["mean_ns"] for point in records if point["owner"] == 0 and point["schema"] == row["schema_id"] and
                 point["capacity"] == row["capacity"] and point["wide"] == row["wide"] and point["layout_identity"] == row["layout_identity"]]
        row["effective_median_ns"] = statistics.median(means)
    for row in rows:
        row.setdefault("effective_median_ns", row["median_ns"])
    for gap in (64, 192):
        ratios: list[float] = []
        baseline_ratios: list[float] = []
        for row in rows:
            if row["owner"] != 0 or row["gap"] != gap:
                continue
            peers = [peer for peer in rows if peer["owner"] == 0 and peer["schema"] == row["schema"] and
                     peer["capacity"] == row["capacity"] and peer["wide"] == row["wide"]]
            ratios.append(row["effective_median_ns"] / min(peer["effective_median_ns"] for peer in peers))
            baseline_ratios.append(row["effective_median_ns"] / next(peer["effective_median_ns"] for peer in peers if peer["gap"] == 0))
        candidates.append(dict(gap=gap, worst_over_best=max(ratios), worst_over_zero=max(baseline_ratios),
                               passes=max(ratios) <= 1.1 and max(baseline_ratios) <= 1.1))
    # Archived pre-policy runs used zero-gap generated controls.
    generated_gaps = {row["gap"] for row in records if row["owner"] == 1}
    generated_gap = 0 if generated_gaps == {0} else 192
    expected = {(row["schema"], row["capacity"], row["wide"], row["owner"], row["gap"])
                for row in confirmation_cases(generated_gap=generated_gap)}
    complete = set(groups) == expected and all(len(points) == 3 and {p["repetition"] for p in points} == {0, 1, 2}
                                              and all(p["samples"] == 100 for p in points) for points in groups.values())
    selected = next((item["gap"] for item in candidates if item["passes"]), None) if complete else None
    variable = [dict(schema=row["schema"], capacity=row["capacity"], wide=row["wide"], owner=row["owner"], gap=row["gap"],
                     maximum_over_minimum=row["maximum_ns"] / row["minimum_ns"])
                for row in rows if row["maximum_ns"] > 1.1 * row["minimum_ns"]]
    return rows, dict(complete=complete, selected_gap=selected, candidates=candidates, equivalent_layouts=equivalent_layouts,
                      variable_groups=variable,
                      interpretation="Candidate for review, not a production change. Inspect process variability before accepting.")


def report(output: Path, records: list[Record]) -> None:
    matplotlib = cast(Any, importlib.import_module("matplotlib"))
    matplotlib.use("Agg")
    plt = cast(Any, importlib.import_module("matplotlib.pyplot"))
    rows, decision = summarize(records)
    with (output / "confirmation-summary.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (output / "confirmation-decision.json").write_text(json.dumps(decision, indent=2), encoding="utf-8")
    for schema in sorted({row["schema"] for row in records}):
        for wide, kernel in ((0, "narrow"), (1, "wide")):
            capacities = sorted({row["capacity"] for row in records if row["schema"] == schema})
            figure, axes = plt.subplots(1, len(capacities), figsize=(4 * len(capacities), 4), squeeze=False, layout="constrained")
            for axis, capacity in zip(axes[0], capacities):
                for repetition in sorted({row["repetition"] for row in records}):
                    points = sorted((row for row in records if row["schema"] == schema and row["capacity"] == capacity and
                                     row["wide"] == wide and row["owner"] == 0 and row["repetition"] == repetition), key=lambda row: row["gap"])
                    axis.errorbar([row["gap"] for row in points], [row["mean_ns"] / 1e6 for row in points],
                                  yerr=([max(0., row["mean_ns"] - row["lower_ns"]) / 1e6 for row in points],
                                        [max(0., row["upper_ns"] - row["mean_ns"]) / 1e6 for row in points]),
                                  marker="o", label=f"Process {repetition + 1}")
                axis.set_title(f"Capacity {capacity}")
                axis.set_xticks(GAPS)
                axis.set_xlabel("Nominal gap (bytes)")
                axis.set_ylabel("Time (ms)")
            axes[0][0].legend()
            suffix = "" if decision["complete"] else " — incomplete/smoke data"
            figure.suptitle(f"{SCHEMAS[schema]}: {kernel}, 65,535 live rows, four passes{suffix}")
            figure.savefig(output / f"{SCHEMAS[schema]}-{kernel}.png", dpi=160)
            plt.close(figure)
