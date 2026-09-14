from __future__ import annotations

from dataclasses import dataclass
from itertools import product
from typing import Any, Iterable, Mapping, cast


CacheVariables = Mapping[str, bool | str]
Preset = dict[str, Any]


@dataclass(frozen=True)
class Compiler:
    name: str
    display_name: str
    base_preset: str
    toolchain_file: str
    supports_asan: bool = False


@dataclass(frozen=True)
class Platform:
    name: str
    display_name: str
    host_system_name: str
    ue_platform: str
    architecture: str
    compilers: tuple[Compiler, ...]


@dataclass(frozen=True)
class Configuration:
    name: str
    display_name: str
    cmake_build_type: str
    ue_configuration: str


@dataclass(frozen=True)
class Feature:
    name: str
    display_name: str
    cache_variables: CacheVariables
    excluded_tests: tuple[str, ...] = ()


@dataclass(frozen=True)
class Combination:
    platform: Platform
    compiler: Compiler
    configuration: Configuration
    features: tuple[Feature, ...]

    @property
    def name(self) -> str:
        components = [
            self.platform.name,
            self.platform.architecture,
            self.compiler.name,
            self.configuration.name,
        ]
        components.extend(feature.name for feature in self.features)
        return "-".join(components)

    @property
    def display_name(self) -> str:
        feature_names = " + ".join(feature.display_name for feature in self.features)
        suffix = f" + {feature_names}" if feature_names else ""
        return (
            f"Native {self.platform.display_name} {self.platform.architecture} "
            f"{self.compiler.display_name} {self.configuration.display_name}{suffix}"
        )

    @property
    def cache_variables(self) -> dict[str, bool | str]:
        values: dict[str, bool | str] = {}
        for feature in self.features:
            values.update(feature.cache_variables)
        return values


@dataclass(frozen=True)
class RejectedCombination:
    category: str
    value: str
    reason: str


EXPLICITLY_UNSUPPORTED = (
    RejectedCombination(
        "platform",
        "linux",
        "The repository has no Linux platform or toolchain definition.",
    ),
    RejectedCombination(
        "architecture",
        "arm64",
        "The Windows toolchains and native SIMD targets are currently x64-only.",
    ),
    RejectedCombination(
        "compiler",
        "clang",
        "Only the Windows MSVC-compatible clang-cl driver is configured.",
    ),
    RejectedCombination(
        "compiler",
        "gcc",
        "The repository has no supported GCC toolchain definition.",
    ),
    RejectedCombination(
        "feature",
        "coverage",
        "No native coverage instrumentation or reporting flow exists.",
    ),
    RejectedCombination(
        "feature",
        "ubsan",
        "UndefinedBehaviorSanitizer has not been validated by the project.",
    ),
)


def expand_matrix(
    platforms: Iterable[Platform],
    configurations: Iterable[Configuration],
    features: tuple[Feature, ...],
) -> tuple[tuple[Combination, ...], tuple[tuple[str, str], ...]]:
    combinations: list[Combination] = []
    rejected: list[tuple[str, str]] = []

    for platform in platforms:
        for compiler in platform.compilers:
            for configuration in configurations:
                for enabled in product((False, True), repeat=len(features)):
                    selected = tuple(
                        feature
                        for feature, is_enabled in zip(features, enabled, strict=True)
                        if is_enabled
                    )
                    combination = Combination(platform, compiler, configuration, selected)

                    if any(feature.name == "asan" for feature in selected) and not compiler.supports_asan:
                        rejected.append(
                            (
                                combination.name,
                                "AddressSanitizer is supported only by the validated clang-cl toolchain.",
                            )
                        )
                        continue

                    combinations.append(combination)

    names = [combination.name for combination in combinations]
    if len(names) != len(set(names)):
        raise ValueError("The native matrix generated duplicate preset names.")

    return tuple(combinations), tuple(rejected)


def validate_preset_references(document: Mapping[str, Any]) -> None:
    configure_presets = cast(list[Preset], document.get("configurePresets", []))
    build_presets = cast(list[Preset], document.get("buildPresets", []))
    test_presets = cast(list[Preset], document.get("testPresets", []))
    workflow_presets = cast(list[Preset], document.get("workflowPresets", []))

    configure_names = _unique_names("configure", configure_presets)
    build_names = _unique_names("build", build_presets)
    test_names = _unique_names("test", test_presets)
    _unique_names("workflow", workflow_presets)

    for preset in build_presets:
        configure_name = cast(str, preset["configurePreset"])
        if configure_name not in configure_names:
            raise ValueError(
                f"Build preset '{preset['name']}' references unknown configure preset "
                f"'{configure_name}'."
            )

    for preset in test_presets:
        configure_name = cast(str, preset["configurePreset"])
        if configure_name not in configure_names:
            raise ValueError(
                f"Test preset '{preset['name']}' references unknown configure preset "
                f"'{configure_name}'."
            )

    known_steps = {
        "configure": configure_names,
        "build": build_names,
        "test": test_names,
    }
    for preset in workflow_presets:
        steps = cast(list[Preset], preset["steps"])
        for step in steps:
            step_type = cast(str, step["type"])
            step_name = cast(str, step["name"])
            if step_type in known_steps and step_name not in known_steps[step_type]:
                raise ValueError(
                    f"Workflow preset '{preset['name']}' references unknown {step_type} "
                    f"preset '{step_name}'."
                )


def _unique_names(kind: str, presets: Iterable[Preset]) -> set[str]:
    names = [cast(str, preset["name"]) for preset in presets]
    if len(names) != len(set(names)):
        raise ValueError(f"The generated document contains duplicate {kind} preset names.")
    return set(names)
