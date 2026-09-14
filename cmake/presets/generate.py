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


def make_unreal_document() -> dict[str, Any]:
    document = generated_header()
    document["include"] = ["base.json"]

    unreal_configurations = (
        ("debug", "Debug", "Debug", "Debug"),
        ("debug-game", "DebugGame", "Debug", "DebugGame"),
        ("development", "Development", "RelWithDebInfo", "Development"),
        ("shipping", "Shipping", "Release", "Shipping"),
        ("test", "Test", "Release", "Test"),
    )
    compiler_variants = (
        ("", "", "windows-clang-cl", "unreal"),
        ("-msvc", " (MSVC native)", "windows-msvc", "unreal-msvc"),
    )

    configure_presets: list[dict[str, Any]] = []
    for _, _, compiler_base, unreal_base in compiler_variants:
        configure_presets.append(
            {
                "name": unreal_base,
                "hidden": True,
                "inherits": compiler_base,
                "cacheVariables": {
                    "CMAKE_C_FLAGS_RELEASE": "/O2 /DNDEBUG",
                    "CMAKE_C_FLAGS_RELWITHDEBINFO": "/O2 /Zi /DNDEBUG",
                    "CMAKE_CXX_FLAGS_RELEASE": "/O2 /DNDEBUG",
                    "CMAKE_CXX_FLAGS_RELWITHDEBINFO": "/O2 /Zi /DNDEBUG",
                    "CMAKE_UNITY_BUILD": True,
                    "CMAKE_UNITY_BUILD_BATCH_SIZE": "32",
                    "SANDBOX_WITH_UNREAL": True,
                },
            }
        )

    for suffix, display_suffix, _, unreal_base in compiler_variants:
        for name, display_name, cmake_build_type, ue_configuration in unreal_configurations:
            configure_presets.append(
                {
                    "name": f"{name}{suffix}",
                    "displayName": f"{display_name}{display_suffix}",
                    "inherits": unreal_base,
                    "cacheVariables": {
                        "CMAKE_BUILD_TYPE": cmake_build_type,
                        "UE_CONFIGURATION": ue_configuration,
                    },
                }
            )

    build_presets: list[dict[str, Any]] = [
        {
            "name": "worktree-dependencies-debug-game",
            "configurePreset": "debug-game",
            "targets": ["worktree-dependencies"],
        },
        {
            "name": "worktree-dependencies-development",
            "configurePreset": "development",
            "targets": ["worktree-dependencies"],
        },
    ]
    for suffix, _, _, _ in compiler_variants:
        for name, _, _, _ in unreal_configurations:
            target = "game" if name == "shipping" else "dev-core"
            build_presets.append(
                {
                    "name": f"{name}{suffix}",
                    "configurePreset": f"{name}{suffix}",
                    "targets": [target],
                }
            )

    extra_build_presets = (
        ("resave-assets", "debug-game", "resave-assets"),
        ("generate-lab-mesh-assemblies", "debug-game", "generate-lab-mesh-assemblies"),
        ("import-game-audio", "debug-game", "import-game-audio"),
        ("generate-project-files", "debug-game", "generate-project-files"),
        ("generate-project-files-development", "development", "generate-project-files"),
        ("format-code", "debug-game", "format-code"),
        ("format-all-code", "debug-game", "format-all-code"),
    )
    build_presets.extend(
        {
            "name": name,
            "configurePreset": configure_preset,
            "targets": [target],
        }
        for name, configure_preset, target in extra_build_presets
    )

    packaging_build_presets = (
        ("development-game", "development", "game"),
        ("development-cook", "development", "cook"),
        ("development-cook-incremental", "development", "cook-incremental"),
        ("development-stage", "development", "stage"),
        ("development-archive", "development", "archive"),
        ("development-run-staged", "development", "run-staged"),
        ("development-verify-package", "development", "verify-package"),
        ("shipping-stage", "shipping", "stage"),
        ("shipping-archive", "shipping", "archive"),
        ("shipping-run-staged", "shipping", "run-staged"),
        ("shipping-verify-package", "shipping", "verify-package"),
    )
    build_presets.extend(
        {
            "name": name,
            "configurePreset": configure_preset,
            "targets": [target],
        }
        for name, configure_preset, target in packaging_build_presets
    )

    test_presets: list[dict[str, Any]] = []
    for suffix in ("", "-msvc"):
        for name, label in (
            ("tests", "^all$"),
            ("level-tests", "^level$"),
            ("unit-tests", "unit"),
        ):
            test_presets.append(
                {
                    "name": f"debug-game{suffix}-{name}",
                    "inherits": "test-base",
                    "configurePreset": f"debug-game{suffix}",
                    "filter": {"include": {"label": label}},
                }
            )
    test_presets.insert(
        1,
        {
            "name": "debug-game-input-smoke-tests",
            "inherits": "test-base",
            "configurePreset": "debug-game",
            "filter": {"include": {"label": "input-smoke"}},
        },
    )

    workflow_presets: list[dict[str, Any]] = [
        {
            "name": "setup-worktree-debug-game",
            "displayName": "Prepare a worktree for DebugGame development",
            "description": "Build DebugGame dependencies, import optional audio, and generate project files",
            "steps": [
                {"type": "configure", "name": "debug-game"},
                {"type": "build", "name": "worktree-dependencies-debug-game"},
                {"type": "build", "name": "import-game-audio"},
                {"type": "build", "name": "generate-project-files"},
            ],
        },
        {
            "name": "setup-worktree-development",
            "displayName": "Prepare a worktree for Development",
            "description": "Build non-Unreal Development dependencies and generate project files",
            "steps": [
                {"type": "configure", "name": "development"},
                {"type": "build", "name": "worktree-dependencies-development"},
                {"type": "build", "name": "generate-project-files-development"},
            ],
        },
    ]
    for suffix, _, _, _ in compiler_variants:
        for name, _, _, _ in unreal_configurations:
            preset_name = f"{name}{suffix}"
            workflow_presets.append(
                {
                    "name": preset_name,
                    "steps": [
                        {"type": "configure", "name": preset_name},
                        {"type": "build", "name": preset_name},
                    ],
                }
            )

    for name, configure_preset, _ in extra_build_presets:
        if name == "generate-project-files-development":
            continue
        workflow_presets.append(
            {
                "name": name,
                "steps": [
                    {"type": "configure", "name": configure_preset},
                    {"type": "build", "name": name},
                ],
            }
        )

    for suffix in ("", "-msvc"):
        for test_suffix in ("tests", "unit-tests"):
            test_name = f"debug-game{suffix}-{test_suffix}"
            workflow_presets.append(
                {
                    "name": test_name,
                    "steps": [
                        {"type": "configure", "name": f"debug-game{suffix}"},
                        {"type": "build", "name": f"debug-game{suffix}"},
                        {"type": "test", "name": test_name},
                    ],
                }
            )

    workflow_presets.extend(
        [
            {
                "name": "development-cook",
                "steps": [
                    {"type": "configure", "name": "development"},
                    {"type": "build", "name": "development-cook"},
                ],
            },
            {
                "name": "development-staged-game",
                "steps": [
                    {"type": "configure", "name": "development"},
                    {"type": "build", "name": "development-game"},
                    {"type": "build", "name": "development-cook-incremental"},
                    {"type": "build", "name": "development-stage"},
                ],
            },
            {
                "name": "development-package",
                "steps": [
                    {"type": "configure", "name": "development"},
                    {"type": "build", "name": "development-game"},
                    {"type": "build", "name": "development-cook"},
                    {"type": "build", "name": "development-stage"},
                    {"type": "build", "name": "development-archive"},
                    {"type": "build", "name": "development-verify-package"},
                ],
            },
            {
                "name": "shipping-package",
                "description": "Build, stage, archive, and verify Shipping using an existing Development cook",
                "steps": [
                    {"type": "configure", "name": "shipping"},
                    {"type": "build", "name": "shipping"},
                    {"type": "build", "name": "shipping-stage"},
                    {"type": "build", "name": "shipping-archive"},
                    {"type": "build", "name": "shipping-verify-package"},
                ],
            },
        ]
    )

    document["configurePresets"] = configure_presets
    document["buildPresets"] = build_presets
    document["testPresets"] = test_presets
    document["workflowPresets"] = workflow_presets
    validate_preset_references(document)
    return document


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
        {
            "name": "native-simulation-benchmark",
            "displayName": "Native simulation benchmark",
            "inherits": BENCHMARK_CONFIGURATION,
            "cacheVariables": {"UE_CONFIGURATION": "Development"},
        },
        {
            "name": "frame-memory-level-benchmark",
            "displayName": "Native frame-memory level benchmark",
            "inherits": BENCHMARK_CONFIGURATION,
            "cacheVariables": {
                "SANDBOX_FRAME_MEMORY_LEVEL_BENCHMARK": True,
                "UE_CONFIGURATION": "Development",
            },
        },
        {
            "name": "telemetry-integration-benchmark",
            "displayName": "Native level telemetry integration benchmark",
            "inherits": BENCHMARK_CONFIGURATION,
            "cacheVariables": {
                "SANDBOX_TELEMETRY_INTEGRATION_BENCHMARK": True,
                "UE_CONFIGURATION": "Shipping",
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
        {
            "name": "native-simulation-benchmark",
            "configurePreset": "native-simulation-benchmark",
            "targets": [
                "native-simulation-benchmark",
                "native-simulation-benchmark-tests",
            ],
        },
        {
            "name": "frame-memory-level-benchmark",
            "configurePreset": "frame-memory-level-benchmark",
            "targets": [
                "native-simulation-benchmark",
                "native-simulation-benchmark-tests",
            ],
        },
        {
            "name": "telemetry-integration-benchmark",
            "configurePreset": "telemetry-integration-benchmark",
            "targets": [
                "native-simulation-benchmark",
                "native-simulation-benchmark-tests",
            ],
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
        {
            "name": "frame-memory-level-benchmark",
            "inherits": "test-base",
            "configurePreset": "frame-memory-level-benchmark",
            "filter": {"include": {"label": "^frame-memory-level-benchmark$"}},
        },
        {
            "name": "telemetry-integration-benchmark",
            "inherits": "test-base",
            "configurePreset": "telemetry-integration-benchmark",
            "filter": {"include": {"label": "^telemetry-integration-benchmark$"}},
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
        {
            "name": "native-simulation-benchmark",
            "steps": [
                {"type": "configure", "name": "native-simulation-benchmark"},
                {"type": "build", "name": "native-simulation-benchmark"},
            ],
        },
        _workflow(
            "frame-memory-level-benchmark",
            "frame-memory-level-benchmark",
            "frame-memory-level-benchmark",
        ),
        _workflow(
            "telemetry-integration-benchmark",
            "telemetry-integration-benchmark",
            "telemetry-integration-benchmark",
        ),
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
    parser = argparse.ArgumentParser(description="Generate CMake preset files.")
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
        PRESET_DIRECTORY / "unreal.json": make_unreal_document(),
        PRESET_DIRECTORY / "native-benchmarks.json": make_native_benchmark_document(),
    }
    stale = False
    for path, document in documents.items():
        stale = update_file(path, serialize(document), arguments.check) or stale

    if arguments.check and stale:
        return 1
    if arguments.check:
        print("Generated CMake presets are up to date.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
