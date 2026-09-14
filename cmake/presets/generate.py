from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import tempfile
from typing import Any

from features import FEATURES, environment_for
from matrix import Configuration, Combination, expand_matrix, validate_preset_references
from platforms import PLATFORMS


PRESET_DIRECTORY = Path(__file__).resolve().parent
GENERATED_VENDOR = {
    "nukethebees.dev/preset-generator": {
        "generated": True,
        "source": "cmake/presets/generate.py",
        "warning": "Generated file. Do not edit manually.",
    }
}

CONFIGURATIONS = (
    Configuration("debug", "Debug", "Debug", "DebugGame"),
    Configuration("release", "Release", "Release", "DebugGame"),
)

CODEGEN_CONFIGURATION = "win-x64-clangcl-debug-unity"
BENCHMARK_CONFIGURATION = "win-x64-clangcl-release-unity"


def generated_header() -> dict[str, Any]:
    return {
        "version": 9,
        "vendor": GENERATED_VENDOR,
    }


def make_base_document() -> dict[str, Any]:
    document = generated_header()
    configure_presets: list[dict[str, Any]] = [
        {
            "name": "base",
            "hidden": True,
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/out/build/${presetName}",
        }
    ]

    for platform in PLATFORMS:
        condition = {
            "type": "equals",
            "lhs": "${hostSystemName}",
            "rhs": platform.host_system_name,
        }
        for compiler in platform.compilers:
            configure_presets.append(
                {
                    "name": compiler.base_preset,
                    "hidden": True,
                    "inherits": "base",
                    "architecture": {
                        "value": platform.architecture,
                        "strategy": "external",
                    },
                    "condition": condition,
                    "toolchainFile": compiler.toolchain_file,
                    "cacheVariables": {"UE_PLATFORM": platform.ue_platform},
                }
            )

    document["configurePresets"] = configure_presets
    document["testPresets"] = [
        {
            "name": "test-base",
            "hidden": True,
            "output": {"outputOnFailure": True},
            "execution": {"noTestsAction": "error"},
        }
    ]
    return document


def make_native_document(combinations: tuple[Combination, ...]) -> dict[str, Any]:
    document = generated_header()
    document["include"] = ["base.json"]

    configure_presets: list[dict[str, Any]] = [
        {
            "name": "native-common",
            "hidden": True,
            "cacheVariables": {
                "SANDBOX_WITH_UNREAL": False,
                "SANDBOX_WITH_ASAN": False,
                "CMAKE_UNITY_BUILD": False,
            },
        }
    ]
    for configuration in CONFIGURATIONS:
        configure_presets.append(
            {
                "name": f"native-config-{configuration.name}",
                "hidden": True,
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": configuration.cmake_build_type,
                    "UE_CONFIGURATION": configuration.ue_configuration,
                },
            }
        )

    for combination in combinations:
        preset: dict[str, Any] = {
            "name": combination.name,
            "displayName": combination.display_name,
            "inherits": [
                combination.compiler.base_preset,
                "native-common",
                f"native-config-{combination.configuration.name}",
            ],
        }
        if combination.cache_variables:
            preset["cacheVariables"] = combination.cache_variables
        environment = environment_for({feature.name for feature in combination.features})
        if environment:
            preset["environment"] = environment
        configure_presets.append(preset)

    document["configurePresets"] = configure_presets
    document["buildPresets"] = [
        *(
            {"name": combination.name, "configurePreset": combination.name}
            for combination in combinations
        ),
        {
            "name": "codegen",
            "configurePreset": CODEGEN_CONFIGURATION,
            "targets": [
                "lispb",
                "codegen-tests",
                "slate-codegen-tests",
                "kernel-codegen-tests",
            ],
        },
        {
            "name": "generate-code",
            "configurePreset": CODEGEN_CONFIGURATION,
            "targets": ["generate-code"],
        },
    ]
    document["testPresets"] = [
        *(
            {
                "name": combination.name,
                "inherits": "test-base",
                "configurePreset": combination.name,
                "filter": test_filter(combination),
            }
            for combination in combinations
        ),
        {
            "name": "codegen-tests",
            "inherits": "test-base",
            "configurePreset": CODEGEN_CONFIGURATION,
            "filter": {"include": {"label": "codegen"}},
        },
    ]
    document["workflowPresets"] = [
        *(
            {
                "name": combination.name,
                "steps": [
                    {"type": "configure", "name": combination.name},
                    {"type": "build", "name": combination.name},
                    {"type": "test", "name": combination.name},
                ],
            }
            for combination in combinations
        ),
        {
            "name": "codegen",
            "steps": [
                {"type": "configure", "name": CODEGEN_CONFIGURATION},
                {"type": "build", "name": "codegen"},
                {"type": "test", "name": "codegen-tests"},
            ],
        },
        {
            "name": "generate-code",
            "steps": [
                {"type": "configure", "name": CODEGEN_CONFIGURATION},
                {"type": "build", "name": "generate-code"},
            ],
        },
    ]

    validate_preset_references(document)
    return document


def test_filter(combination: Combination) -> dict[str, Any]:
    test_filter: dict[str, Any] = {"include": {"label": "all"}}
    exclusions = tuple(
        test_name
        for feature in combination.features
        for test_name in feature.excluded_tests
    )
    if exclusions:
        names = "|".join(re.escape(test_name) for test_name in exclusions)
        test_filter["exclude"] = {"name": f"^({names})$"}
    return test_filter


def make_native_benchmark_document() -> dict[str, Any]:
    document = generated_header()
    document["include"] = ["native.json"]
    document["configurePresets"] = [
        {
            "name": "kernel-benchmark",
            "displayName": "Native kernel benchmark",
            "inherits": BENCHMARK_CONFIGURATION,
            "cacheVariables": {
                "SANDBOX_KERNEL_BENCHMARKS": True,
                "UE_CONFIGURATION": "Development",
            },
        },
        {
            "name": "kernel-benchmark-plots",
            "displayName": "Native kernel benchmark plots",
            "inherits": "kernel-benchmark",
            "cacheVariables": {"SANDBOX_KERNEL_BENCHMARK_PLOTS": True},
        },
        {
            "name": "native-soa",
            "inherits": BENCHMARK_CONFIGURATION,
            "cacheVariables": {
                "SANDBOX_NATIVE_SOA_BENCHMARKS": True,
                "UE_CONFIGURATION": "Development",
            },
        },
    ]
    document["buildPresets"] = [
        {
            "name": "kernel-benchmark",
            "configurePreset": "kernel-benchmark",
            "targets": ["kernel-native-simd-tests", "kernel-native-benchmarks"],
        },
        {
            "name": "kernel-benchmark-plots",
            "configurePreset": "kernel-benchmark-plots",
            "targets": ["kernel-native-simd-tests", "kernel-benchmark-report"],
        },
        {
            "name": "kernel-benchmark-plots-full",
            "configurePreset": "kernel-benchmark-plots",
            "targets": ["kernel-native-simd-tests", "kernel-benchmark-report-full"],
        },
        {
            "name": "kernel-vector-layout-benchmark-plots",
            "configurePreset": "kernel-benchmark-plots",
            "targets": [
                "kernel-native-simd-tests",
                "kernel-vector-layout-benchmark-report",
            ],
        },
        {
            "name": "native-soa",
            "configurePreset": "native-soa",
            "targets": [
                "native-soa-tests",
                "native-soa-tests-mimalloc",
                "native-soa-benchmarks",
                "native-soa-reserve-matrix",
                "native-soa-reserve-matrix-mimalloc",
                "codegen-tests",
            ],
        },
        {
            "name": "native-soa-reserve",
            "configurePreset": "native-soa",
            "targets": ["native-soa-reserve-report"],
        },
        {
            "name": "native-soa-reserve-matrix",
            "configurePreset": "native-soa",
            "targets": ["native-soa-reserve-matrix-report"],
        },
    ]
    document["testPresets"] = [
        {
            "name": "kernel-benchmark-tests",
            "inherits": "test-base",
            "configurePreset": "kernel-benchmark",
            "filter": {"include": {"label": "native-kernel-benchmark"}},
        },
        {
            "name": "kernel-benchmark-plot-tests",
            "inherits": "test-base",
            "configurePreset": "kernel-benchmark-plots",
            "filter": {
                "include": {
                    "label": "native-kernel-benchmark|kernel-benchmark-plot"
                }
            },
        },
        {
            "name": "native-soa",
            "inherits": "test-base",
            "configurePreset": "native-soa",
            "filter": {
                "include": {
                    "name": "^(native-soa|SingleAllocationSoa\\.|Json\\.LoadsExperimentalStdlibSoa)"
                }
            },
        },
    ]
    document["workflowPresets"] = [
        _workflow("kernel-benchmark", "kernel-benchmark", "kernel-benchmark-tests"),
        _workflow(
            "kernel-benchmark-plots",
            "kernel-benchmark-plots",
            "kernel-benchmark-plot-tests",
        ),
        _workflow(
            "kernel-benchmark-plots-full",
            "kernel-benchmark-plots",
            "kernel-benchmark-plot-tests",
            build_name="kernel-benchmark-plots-full",
        ),
        _workflow(
            "kernel-vector-layout-benchmark-plots",
            "kernel-benchmark-plots",
            "kernel-benchmark-plot-tests",
            build_name="kernel-vector-layout-benchmark-plots",
        ),
        _workflow("native-soa", "native-soa", "native-soa"),
        {
            "name": "native-soa-reserve",
            "steps": [
                {"type": "configure", "name": "native-soa"},
                {"type": "build", "name": "native-soa"},
                {"type": "test", "name": "native-soa"},
                {"type": "build", "name": "native-soa-reserve"},
            ],
        },
        {
            "name": "native-soa-reserve-matrix",
            "steps": [
                {"type": "configure", "name": "native-soa"},
                {"type": "build", "name": "native-soa"},
                {"type": "test", "name": "native-soa"},
                {"type": "build", "name": "native-soa-reserve-matrix"},
            ],
        },
    ]
    return document


def _workflow(
    name: str,
    configure_name: str,
    test_name: str,
    *,
    build_name: str | None = None,
) -> dict[str, Any]:
    return {
        "name": name,
        "steps": [
            {"type": "configure", "name": configure_name},
            {"type": "build", "name": build_name or name},
            {"type": "test", "name": test_name},
        ],
    }


def serialize(document: dict[str, Any]) -> str:
    return json.dumps(document, indent=2, ensure_ascii=False) + "\n"


def update_file(path: Path, contents: str, check: bool) -> bool:
    existing = path.read_text(encoding="utf-8") if path.exists() else None
    if existing == contents:
        return False
    if check:
        print(f"Out of date: {path.relative_to(PRESET_DIRECTORY.parent.parent)}")
        return True

    with tempfile.NamedTemporaryFile(
        "w",
        encoding="utf-8",
        newline="\n",
        dir=path.parent,
        prefix=f".{path.name}.",
        delete=False,
    ) as temporary_file:
        temporary_file.write(contents)
        temporary_path = Path(temporary_file.name)
    os.replace(temporary_path, path)
    print(f"Generated {path.relative_to(PRESET_DIRECTORY.parent.parent)}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate native CMake preset files.")
    parser.add_argument(
        "--check",
        action="store_true",
        help="Report stale generated files without updating them.",
    )
    arguments = parser.parse_args()

    combinations, rejected = expand_matrix(PLATFORMS, CONFIGURATIONS, FEATURES)
    if len(combinations) != 12:
        raise ValueError(f"Expected 12 supported native combinations, got {len(combinations)}.")
    if len(rejected) != 4:
        raise ValueError(f"Expected 4 explicitly rejected MSVC ASan combinations, got {len(rejected)}.")

    documents = {
        PRESET_DIRECTORY / "base.json": make_base_document(),
        PRESET_DIRECTORY / "native.json": make_native_document(combinations),
        PRESET_DIRECTORY / "native-benchmarks.json": make_native_benchmark_document(),
    }
    stale = False
    for path, document in documents.items():
        stale = update_file(path, serialize(document), arguments.check) or stale

    if arguments.check and stale:
        return 1
    if arguments.check:
        print("Generated native CMake presets are up to date.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
