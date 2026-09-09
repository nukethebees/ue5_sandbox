# /// script
# requires-python = ">=3.11"
# dependencies = ["matplotlib>=3.10,<4"]
# ///
"""Run isolated Catch2 column-spacing cases and preserve a reproducible staged report."""
from __future__ import annotations

import argparse
import csv
import datetime
import json
import importlib
import hashlib
import os
import platform
import random
import shutil
import re
import subprocess
from pathlib import Path
from typing import Any, cast
import soa_spacing_confirmation as confirmation_suite

ROOT = Path(__file__).resolve().parent.parent
CAPACITIES = (65536, 65600, 75008, 100032, 131072)
GAPS = (0, 64, 128, 192, 256, 384, 512, 768, 1024)
OWNERS = {0: "Spaced", 1: "Generated", 2: "TArray"}
Record = dict[str, Any]


def command_output(command: list[str]) -> str:
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, check=False)
    return result.stdout + result.stderr


def case(capacity: int, gap: int, owner: int, wide: int, live: int = 65535) -> Record:
    return dict(capacity=capacity, gap=192 if owner == 1 else gap, owner=owner, wide=wide, live=live)


def initial_cases() -> list[Record]:
    cases = [case(capacity, gap, 0, wide) for capacity in CAPACITIES for gap in GAPS for wide in (0, 1)]
    cases += [case(capacity, 0, 1, wide) for capacity in CAPACITIES for wide in (0, 1)]
    cases += [case(65536, 0, 2, wide) for wide in (0, 1)]
    cases += [case(capacity, 0, owner, wide, 65536)
              for capacity, owner in ((65536, 2), (65536, 1), (65600, 1)) for wide in (0, 1)]
    return cases


def run_case(item: Record, output: Path, number: int, samples: int, repetition: int, smoke: bool = False,
             cpu: tuple[int, int] | None = None) -> Record:
    env = dict(os.environ)
    for field, value in item.items():
        env[f"SOA_SPACING_{field.upper()}"] = str(value)
    executable = ROOT / "Binaries/Win64/SandboxCoreBenchmarks/SandboxCoreBenchmarks.exe"
    test = "Confirmation" if cpu else "Measure"
    if cpu:
        env["SOA_SPACING_CPU_GROUP"], env["SOA_SPACING_CPU"] = map(str, cpu)
    command = [str(executable), f"SandboxCore.SingleAllocation.Spacing.{test}", "--benchmark-samples", str(samples)]
    if smoke:
        command += ["--benchmark-warmup-time", "0", "--benchmark-no-analysis"]
    print(f"[{number}] repetition={repetition} {item}", flush=True)
    result = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, check=False)
    log = result.stdout + result.stderr
    (output / f"case-{number:04d}.log").write_text(log, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"Case {number} failed; see saved log")
    timing = re.search(r"^SPACING_TIMING,(.*)$", log, re.MULTILINE)
    allocation = re.search(r"^SPACING_ALLOCATION,(.*)$", log, re.MULTILINE)
    if not timing or not allocation:
        raise ValueError(f"Missing diagnostics for case {number}")
    mean, lower, upper, iterations, actual_samples = map(float, timing.group(1).split(","))
    base, total, actual_capacity = map(int, allocation.group(1).split(","))
    observed_cpu: dict[str, str] = {}
    if cpu:
        for match in re.finditer(r"^SPACING_CPU,(before|after),(.*)$", log, re.MULTILINE):
            requested_group, requested_cpu, actual_group, actual_cpu = map(int, match.group(2).split(","))
            if (requested_group, requested_cpu) != cpu or (actual_group, actual_cpu) != cpu:
                raise ValueError(f"CPU affinity mismatch in case {number}")
            observed_cpu[match.group(1)] = f"{actual_group}:{actual_cpu}"
        if set(observed_cpu) != {"before", "after"}:
            raise ValueError(f"Missing CPU verification in case {number}")
    columns: list[Record] = []
    for match in re.finditer(r"^SPACING_COLUMN,(.*)$", log, re.MULTILINE):
        values = list(map(int, match.group(1).split(",")))
        ordinal, address, size, alignment, offset = values[:5]
        column_capacity = values[5] if len(values) == 6 else actual_capacity
        columns.append(dict(ordinal=ordinal, address=address, element_size=size, alignment=alignment,
                            offset=offset, extent=column_capacity * size, column_capacity=column_capacity,
                            mod64=address % 64, mod4096=address % 4096, mod65536=address % 65536))
    for index, column in enumerate(columns):
        column["next_stride"] = columns[index + 1]["address"] - column["address"] if index + 1 < len(columns) else None
        column["next_gap"] = column["next_stride"] - column["extent"] if column["next_stride"] is not None else None
    paths = confirmation_suite.field_paths(ROOT, item.get("schema", 0))
    if len(paths) != len(columns):
        raise ValueError("Column diagnostics do not match the schema")
    zero_bytes = 0
    for path, column in zip(paths, columns):
        column["field_path"] = path
        alignment = column["alignment"]
        zero_bytes = ((zero_bytes + alignment - 1) // alignment) * alignment + column["extent"]
    layout = [(column["offset"], column["extent"], column["alignment"]) for column in columns]
    identity = hashlib.sha256(json.dumps(layout).encode()).hexdigest() if base else "separate-arrays"
    return {**item, "number": number, "repetition": repetition, "mean_ns": mean, "lower_ns": lower,
            "upper_ns": upper, "iterations": iterations, "samples": actual_samples, "base": base,
            "bytes": total, "extra_bytes": total - sum(column["extent"] for column in columns) if item["owner"] != 2 else None,
            "actual_capacity": actual_capacity, "columns": columns,
            "schema_name": confirmation_suite.SCHEMAS[item.get("schema", 0)],
            "layout_identity": identity, "layout_policy": "sequential" if item["owner"] == 0 else "generated-192" if base else "TArray",
            "gap_extra_bytes": total - zero_bytes if item["owner"] == 0 else None,
            "cpu_group": cpu[0] if cpu else None, "cpu": cpu[1] if cpu else None,
            "cpu_before": observed_cpu.get("before"), "cpu_after": observed_cpu.get("after")}


def write_csv(path: Path, rows: list[Record]) -> None:
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def report(output: Path, records: list[Record]) -> None:
    matplotlib = cast(Any, importlib.import_module("matplotlib"))
    matplotlib.use("Agg")
    plt = cast(Any, importlib.import_module("matplotlib.pyplot"))

    write_csv(output / "timings.csv", [{k: v for k, v in row.items() if k != "columns"} for row in records])
    write_csv(output / "columns.csv", [{"case": row["number"], **column} for row in records for column in row["columns"]])
    if "schema" in records[0]:
        confirmation_suite.report(output, records)
        return
    first = [row for row in records if row["repetition"] == 0 and row["owner"] == 0]
    for wide, name in ((0, "narrow"), (1, "wide")):
        figure, axes = plt.subplots(1, 2, figsize=(15, 5), layout="constrained")
        matrix = [[next(row["mean_ns"] / 1e6 for row in first
                        if row["capacity"] == capacity and row["gap"] == gap and row["wide"] == wide)
                   for gap in GAPS] for capacity in CAPACITIES]
        chart = axes[0].imshow(matrix, aspect="auto")
        axes[0].set_xticks(range(len(GAPS)), [str(gap) for gap in GAPS])
        axes[0].set_yticks(range(len(CAPACITIES)), [str(capacity) for capacity in CAPACITIES])
        axes[0].set_xlabel("Gap between columns (bytes)")
        axes[0].set_ylabel("Capacity; 65,535 live rows")
        figure.colorbar(chart, ax=axes[0], label="Mean time (ms)")
        for capacity, times in zip(CAPACITIES, matrix):
            axes[1].plot(GAPS, times, marker="o", label=str(capacity))
        axes[1].set_xlabel("Gap (bytes)")
        axes[1].set_ylabel("Mean time (ms)")
        axes[1].legend(title="Capacity")
        figure.suptitle(f"{name.title()} iteration — initial sweep, four passes")
        figure.savefig(output / f"{name}.png", dpi=160)
        plt.close(figure)
        repeated = [row for row in records if row["repetition"] > 0 and row["owner"] == 0 and row["wide"] == wide]
        if repeated:
            figure, axes = plt.subplots(1, len(CAPACITIES), figsize=(18, 4), layout="constrained")
            for axis, capacity in zip(axes, CAPACITIES):
                selected = [row for row in repeated if row["capacity"] == capacity]
                for repetition in (1, 2):
                    points = sorted((row for row in selected if row["repetition"] == repetition), key=lambda row: row["gap"])
                    axis.errorbar([row["gap"] for row in points], [row["mean_ns"] / 1e6 for row in points],
                                  yerr=([ (row["mean_ns"] - row["lower_ns"]) / 1e6 for row in points],
                                        [ (row["upper_ns"] - row["mean_ns"]) / 1e6 for row in points]),
                                  marker="o", label=f"Process repeat {repetition}")
                axis.set_title(str(capacity))
                axis.set_xlabel("Gap (bytes)")
                axis.set_ylabel("Mean time (ms)")
            axes[0].legend()
            figure.suptitle(f"{name.title()} — independent confirmation processes")
            figure.savefig(output / f"{name}-confirmation.png", dpi=160)
            plt.close(figure)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--seed", type=int, default=20260908)
    parser.add_argument("--suite", choices=("sweep", "confirmation"), default="sweep")
    parser.add_argument("--cpu-group", type=int, default=0)
    parser.add_argument("--cpu", type=int, default=2)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--report-only", action="store_true")
    parser.add_argument("--smoke", action="store_true", help="Validate the full pipeline with one unanalysed sample; not performance evidence")
    args = parser.parse_args()
    pinned = args.suite == "confirmation"
    if args.cpu_group < 0 or not 0 <= args.cpu < 64:
        parser.error("CPU group must be nonnegative and CPU must be in [0, 63]")
    cases = confirmation_suite.confirmation_cases(args.smoke) if pinned else initial_cases()
    rng = random.Random(args.seed)
    rng.shuffle(cases)
    if args.dry_run:
        print(json.dumps(cases, indent=2))
        print(f"{len(cases) * (3 if pinned and not args.smoke else 1)} isolated processes" +
              ("; fixed-CPU confirmation." if pinned else "; confirmation follows zero/best/worst selection."))
        return
    output: Path = args.output_dir or ROOT / ".local/benchmarks/single-allocation/spacing" / datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    if args.smoke:
        output = output / datetime.datetime.now().strftime("smoke-%Y%m%d-%H%M%S")
    if args.report_only:
        report(output, json.loads((output / "results.json").read_text(encoding="utf-8")))
        return
    output.mkdir(parents=True, exist_ok=True)
    if (output / "metadata.json").exists():
        raise ValueError("Output already contains a run; choose a new --output-dir to preserve it")
    metadata = dict(seed=args.seed, smoke=args.smoke, suite=args.suite,
                    selected_cpu=f"{args.cpu_group}:{args.cpu}" if pinned else None,
                    started=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                    platform=platform.platform(), processor=platform.processor(),
                    cpu=command_output(["powershell", "-NoProfile", "-Command", "Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors | ConvertTo-Json"]),
                    commit=command_output(["git", "rev-parse", "HEAD"]),
                    status=command_output(["git", "status", "--short"]),
                    reserve_requests=[65535, 65536, 65537, 75000, 100000, 131072],
                    configuration="Unreal Development; MimallocStorageAllocator; benchmark target toolchain")
    (output / "metadata.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    (output / "working-tree.diff").write_text(command_output(["git", "diff", "HEAD"]), encoding="utf-8")
    for path in (Path(__file__), ROOT / "Plugins/SandboxCore/Tests/SandboxCoreBenchmarks/Private/single_allocation_soa_spacing.cpp"):
        (output / path.name).write_bytes(path.read_bytes())
    for relative in ("Scripts/soa_spacing_confirmation.py", "Codegen/manifests/soa_spacing_shapes.json",
                     "Codegen/manifests/single_allocation_experiment.json",
                     "Plugins/SandboxCore/Tests/SandboxCoreBenchmarks/Private/single_allocation_soa_spacing_support.h",
                     "Plugins/SandboxCore/Tests/SandboxCoreBenchmarks/Private/single_allocation_soa_confirmation.cpp",
                     "Plugins/SandboxCore/Source/SbxCoreExperiments/Public/SbxCoreExperiments/soa_spacing_shapes.h"):
        path = ROOT / relative
        (output / path.name).write_bytes(path.read_bytes())
    binaries = ROOT / "Binaries/Win64/SandboxCoreBenchmarks"
    binary_metadata: Record = {}
    for name in ("SandboxCoreBenchmarks.exe", "sbx-mimalloc.dll"):
        path = binaries / name
        binary_metadata[name] = {"sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "bytes": path.stat().st_size}
    (output / "binaries.json").write_text(json.dumps(binary_metadata, indent=2), encoding="utf-8")
    object_file = ROOT / "Intermediate/Build/Win64/x64/SandboxCoreBenchmarks/Development/SandboxCoreBenchmarks/single_allocation_soa_benchmarks.cpp.obj"
    objdump = shutil.which("llvm-objdump")
    if objdump and object_file.exists():
        symbol = "?iterate@single_allocation_benchmarks@ml@@YAXUEntityDataView@single_allocation_experiment@2@_N@Z"
        assembly = command_output([objdump, f"--disassemble-symbols={symbol}", "--no-show-raw-insn", str(object_file)])
        (output / "iteration-assembly.txt").write_text(assembly, encoding="utf-8")
        response = object_file.with_suffix(".obj.rsp")
        if response.exists():
            (output / "iteration-compiler-arguments.rsp").write_bytes(response.read_bytes())
    shape_object = object_file.with_name("single_allocation_soa_confirmation.cpp.obj")
    if objdump and shape_object.exists():
        symbol_table = command_output([objdump, "--syms", str(shape_object)])
        symbols = [line.split()[-1] for line in symbol_table.splitlines() if line.split() and
                   line.split()[-1].startswith("?iterate@soa_spacing_experiment@ml@@")]
        if symbols:
            assembly = command_output([objdump, "--disassemble-symbols=" + ",".join(symbols), "--no-show-raw-insn", str(shape_object)])
            (output / "shape-iteration-assembly.txt").write_text(assembly, encoding="utf-8")
        response = shape_object.with_suffix(".obj.rsp")
        if response.exists():
            (output / "shape-compiler-arguments.rsp").write_bytes(response.read_bytes())
    for name in ("CMakeCache.txt", "vcpkg-manifest-install.log"):
        source = ROOT / "out/build/benchmark" / name
        if source.exists():
            (output / name).write_bytes(source.read_bytes())
    cache = (ROOT / "out/build/benchmark/CMakeCache.txt").read_text(encoding="utf-8")
    engine = re.search(r"^UE_ROOT:PATH=(.*)$", cache, re.MULTILINE)
    if engine:
        build_log = Path(engine.group(1).strip()) / "Engine/Programs/UnrealBuildTool/Log.txt"
        if build_log.exists():
            (output / "unreal-build.log").write_bytes(build_log.read_bytes())
    installed = ROOT / "vcpkg_installed/vcpkg/status"
    if installed.exists():
        (output / "vcpkg-status.txt").write_bytes(installed.read_bytes())
    records: list[Record] = []
    def run_batch(batch: list[Record], samples: int, repetition: int) -> None:
        for item in batch:
            records.append(run_case(item, output, len(records) + 1, samples, repetition, args.smoke,
                                    (args.cpu_group, args.cpu) if pinned else None))
            (output / "results.json").write_text(json.dumps(records, indent=2), encoding="utf-8")
    if pinned:
        for repetition in range(1 if args.smoke else 3):
            rng.shuffle(cases)
            run_batch(cases, 1 if args.smoke else 100, repetition)
        report(output, records)
        print(f"Saved {'pipeline smoke (not performance evidence)' if args.smoke else 'fixed-CPU confirmation'} to {output}")
        return
    run_batch(cases, 1 if args.smoke else 30, 0)
    if args.smoke:
        report(output, records)
        print(f"Pipeline smoke passed (not timing evidence): {output}")
        return
    confirmation: list[Record] = []
    for capacity in CAPACITIES:
        candidates = sorted((row for row in records if row["owner"] == 0 and row["capacity"] == capacity and row["wide"] == 1),
                            key=lambda row: row["mean_ns"])
        gaps = sorted({0, candidates[0]["gap"], candidates[-1]["gap"]})
        confirmation += [case(capacity, gap, 0, wide) for gap in gaps for wide in (0, 1)]
        confirmation += [case(capacity, 0, 1, wide) for wide in (0, 1)]
    confirmation += [case(65536, 0, 2, wide) for wide in (0, 1)]
    for repetition in (1, 2):
        rng.shuffle(confirmation)
        run_batch(confirmation, 100, repetition)
    report(output, records)
    print(f"Saved results and PNG plots to {output}")


if __name__ == "__main__":
    main()
