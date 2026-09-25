from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

from matrix import validate_preset_references

PRESET_DIRECTORY = Path(__file__).resolve().parent
TIDY_SCOPES = {
    "core": ("core", "profiling"),
    "simulation": ("simulation", "simulation_benchmark"),
    "layout": ("layout",),
    "lispb": ("lispb",),
    "memory": ("memory",),
    "level-authoring": ("level_authoring",),
    "s7": ("s7",),
    "image": ("image",),
    "mesh-gen": ("mesh_gen",),
}


class NativeWorkflowTests(unittest.TestCase):
    source_dir: Path
    cmake: str

    presets = json.loads((PRESET_DIRECTORY / "native.json").read_text(encoding="utf-8"))
    unreal_presets = json.loads(
        (PRESET_DIRECTORY / "unreal.json").read_text(encoding="utf-8")
    )

    def test_tidy_presets_share_one_configure_tree(self) -> None:
        validate_preset_references(self.presets)
        configure_name = "win-x64-clangcl-debug-tidy"
        configure_presets = self.presets["configurePresets"]
        self.assertEqual(
            [preset["name"] for preset in configure_presets if "tidy" in preset["name"]],
            [configure_name],
        )
        tidy_configuration = next(
            preset for preset in configure_presets if preset["name"] == configure_name
        )
        self.assertEqual(
            tidy_configuration["binaryDir"],
            "${sourceDir}/out/build/win-x64-clangcl-debug/clang-tidy",
        )
        self.assertEqual(
            tidy_configuration["cacheVariables"],
            {
                "IOJ_ENABLE_CLANG_TIDY": True,
                "CMAKE_DISABLE_PRECOMPILE_HEADERS": True,
                "CMAKE_CXX_SCAN_FOR_MODULES": False,
                "CMAKE_EXPORT_COMPILE_COMMANDS": True,
            },
        )
        build_presets = {preset["name"]: preset for preset in self.presets["buildPresets"]}
        workflows = {preset["name"]: preset for preset in self.presets["workflowPresets"]}
        targets = {configure_name: "native-clang-tidy"}
        targets.update(
            (f"clang-tidy-{scope}", f"native-clang-tidy-{scope}")
            for scope in TIDY_SCOPES
        )
        self.assertEqual(
            {name for name in build_presets if "tidy" in name}, set(targets)
        )
        self.assertEqual({name for name in workflows if "tidy" in name}, set(targets))
        for name, target in targets.items():
            with self.subTest(preset=name):
                self.assertEqual(build_presets[name]["configurePreset"], configure_name)
                self.assertEqual(build_presets[name]["targets"], [target])
                self.assertEqual(
                    workflows[name]["steps"],
                    [
                        {"type": "configure", "name": configure_name},
                        {"type": "build", "name": name},
                    ],
                )

    def test_tidy_targets_and_source_filters(self) -> None:
        # Exercise the real CMake targets, capturing their runner arguments without
        # requiring LLVM or configuring the native dependency graph.
        with tempfile.TemporaryDirectory(prefix="sandbox tidy workflow ") as root:
            fixture = Path(root)
            build = fixture / "build with spaces"
            (fixture / "capture.py").write_text(
                "import json, pathlib, sys\n"
                "args = sys.argv[1:]\n"
                "log = pathlib.Path(args[args.index('-LogFile') + 1])\n"
                "log.with_suffix('.json').write_text(json.dumps(args), encoding='utf-8')\n",
                encoding="utf-8",
            )
            (fixture / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.28)\n"
                "project(TidyWorkflow NONE)\n"
                f'set(PROJECT_SOURCE_DIR "{self.source_dir.as_posix()}")\n'
                "set(IOJ_ENABLE_CLANG_TIDY TRUE)\n"
                "set(IOJ_IS_CLANG_CL TRUE)\n"
                f'set(Python3_EXECUTABLE "{Path(sys.executable).as_posix()}")\n'
                'set(IOJ_CLANG_TIDY_EXECUTABLE "${CMAKE_COMMAND}")\n'
                'set(IOJ_RUN_CLANG_TIDY_EXECUTABLE "${CMAKE_COMMAND}")\n'
                'set(IOJ_POWERSHELL_EXECUTABLE "${CMAKE_COMMAND}")\n'
                "function(sandbox_jobserver_command output)\n"
                '  set(${output} "${Python3_EXECUTABLE}" '
                '"${CMAKE_CURRENT_SOURCE_DIR}/capture.py" PARENT_SCOPE)\n'
                "endfunction()\n"
                'include("${PROJECT_SOURCE_DIR}/cmake/clang_tidy/CMakeLists.txt")\n'
                "sandbox_configure_native_clang_tidy()\n"
                "sandbox_add_native_clang_tidy_target()\n",
                encoding="utf-8",
            )
            self.run_cmake("-S", str(fixture), "-B", str(build), "-G", "Ninja")
            targets = [
                target
                for preset in self.presets["buildPresets"]
                if "tidy" in preset["name"]
                for target in preset["targets"]
            ]
            self.run_cmake("--build", str(build), "--target", *targets)
            filters = {}
            for name in ("clang-tidy", *(f"clang-tidy-{scope}" for scope in TIDY_SCOPES)):
                args = json.loads((build / f"{name}.json").read_text(encoding="utf-8"))
                self.assertEqual(Path(args[args.index("-CompilationDatabase") + 1]), build)
                self.assertEqual(args[args.index("-Jobs") + 1], "0")
                self.assertEqual(
                    Path(args[args.index("-File") + 1]),
                    self.source_dir / "cmake/clang_tidy/run_clang_tidy.ps1",
                )
                filters[name] = re.compile(args[args.index("-SourceFilter") + 1])

        full_filter = filters.pop("clang-tidy")
        included = {
            f"{directory}/src/example.cpp": scope
            for scope, directories in TIDY_SCOPES.items()
            for directory in directories
        }
        # File names containing 'generated' alone have never been excluded.
        included["lispb/tests/generated_tests.cpp"] = "lispb"
        excluded = (
            "third_party/example/src/example.cpp",
            "sbx_mimalloc/src/example.cpp",
            "s7/lib/src/s7_sandbox.cpp",
            "lispb/kernel/tests/standard_anchor_tests.cpp",
            "simulation/src/lasers/phase_interface.cpp",
            "simulation/src/fighters/phase_interface.cpp",
            "core/src/generated/example.cpp",
            "core/src/generated_kernels/example.cpp",
            "core/tests/compile/example.cpp",
            "lispb/tests/compile_fixture/example.cpp",
            "core/tests/static_string_view_reject.cpp",
            "core/include/example.h",
        )
        native_root = self.source_dir / "native"
        for separator in ("/", "\\"):
            for relative, scope in included.items():
                path = (native_root / relative).as_posix().replace("/", separator)
                with self.subTest(path=path):
                    self.assertIsNotNone(full_filter.search(path))
                    self.assertEqual(
                        [name for name, pattern in filters.items() if pattern.search(path)],
                        [f"clang-tidy-{scope}"],
                    )
            for relative in excluded:
                path = (native_root / relative).as_posix().replace("/", separator)
                with self.subTest(path=path):
                    self.assertIsNone(full_filter.search(path))
                    self.assertFalse(any(pattern.search(path) for pattern in filters.values()))
            outside = (self.source_dir / "tools/example.cpp").as_posix().replace("/", separator)
            self.assertIsNone(full_filter.search(outside))
            unscoped = (native_root / "core_extra/src/example.cpp").as_posix().replace("/", separator)
            self.assertIsNotNone(full_filter.search(unscoped))
            self.assertFalse(any(pattern.search(unscoped) for pattern in filters.values()))

        for source in native_root.rglob("*.cpp"):
            path = source.as_posix()
            with self.subTest(source=path):
                selected = sum(bool(pattern.search(path)) for pattern in filters.values())
                self.assertEqual(selected, int(bool(full_filter.search(path))))

    def test_native_preset_uses_the_unreal_disabled_default(self) -> None:
        configure_presets = {
            preset["name"]: preset for preset in self.presets["configurePresets"]
        }

        native = configure_presets["native"]
        self.assertEqual(native["inherits"], "win-x64-clangcl-debug-unity")
        self.assertFalse(
            configure_presets["native-common"]["cacheVariables"]["IOJ_WITH_UNREAL"]
        )

        for path in PRESET_DIRECTORY.glob("*.json"):
            document = json.loads(path.read_text(encoding="utf-8"))
            for preset in document.get("configurePresets", []):
                with self.subTest(file=path.name, preset=preset["name"]):
                    self.assertFalse(
                        any(
                            key.startswith("SANDBOX_")
                            for key in preset.get("cacheVariables", {})
                        ),
                        "Project build cache keys must use the IOJ_ prefix",
                    )

    def test_native_workflows_build_and_run_native_only_validation(self) -> None:
        build_presets = {preset["name"]: preset for preset in self.presets["buildPresets"]}
        test_presets = {preset["name"]: preset for preset in self.presets["testPresets"]}
        workflows = {preset["name"]: preset for preset in self.presets["workflowPresets"]}

        self.assertEqual(build_presets["native"]["targets"], ["native-tests"])
        self.assertEqual(
            test_presets["native-tests"]["filter"]["include"]["label"], "^native$"
        )
        self.assertEqual(
            workflows["native-tests"]["steps"],
            [
                {"type": "configure", "name": "native"},
                {"type": "build", "name": "native"},
                {"type": "test", "name": "native-tests"},
            ],
        )

    def test_tool_workflow_is_explicit_and_normal_workflows_exclude_it(self) -> None:
        test_presets = {preset["name"]: preset for preset in self.presets["testPresets"]}
        workflows = {preset["name"]: preset for preset in self.presets["workflowPresets"]}
        unreal_test_presets = {
            preset["name"]: preset for preset in self.unreal_presets["testPresets"]
        }

        self.assertEqual(
            test_presets["tool-tests"]["filter"]["include"]["label"],
            "^developer-tool$",
        )
        self.assertEqual(
            workflows["tool-tests"]["steps"],
            [
                {"type": "configure", "name": "native"},
                {"type": "test", "name": "tool-tests"},
            ],
        )
        self.assertEqual(
            unreal_test_presets["debug-game-tests"]["filter"]["include"]["label"],
            "^all$",
        )
        self.assertEqual(
            unreal_test_presets["debug-game-full-tests"]["filter"]["include"]["label"],
            "^(all|developer-tool)$",
        )

    def test_native_target_dry_run_has_no_unreal_dependency(self) -> None:
        generated_sources = self.create_generated_source_sentinels()
        with tempfile.TemporaryDirectory(
            prefix="sandbox native workflow "
        ) as temporary_root:
            build_directory = Path(temporary_root) / "build with spaces"
            self.run_cmake(
                "-S",
                str(self.source_dir),
                "-B",
                str(build_directory),
                "-G",
                "Ninja",
                "-DCMAKE_BUILD_TYPE=Debug",
                "-DCMAKE_UNITY_BUILD=ON",
                "-DIOJ_WITH_UNREAL=OFF",
            )

            cache = (build_directory / "CMakeCache.txt").read_text(encoding="utf-8")
            self.assertIn("IOJ_WITH_UNREAL:BOOL=OFF", cache)

            dry_run = self.run_cmake(
                "--build",
                str(build_directory),
                "--target",
                "native-tests",
                "--",
                "-t",
                "commands",
            )
            self.assertNotRegex(dry_run, r"UnrealBuildTools|UnrealEditor|RunUBT")
            self.assertNotRegex(dry_run, r"(?i)(?:^|[\\/\s])unreal(?:[\\/\s]|$)")
            self.assertNotIn("Tools.slnx", dry_run)
            self.assertNotIn("dotnet.exe\" test", dry_run)

            host_tool = (
                build_directory
                / "host-tools"
                / "NativeBinaryTools"
                / "Debug"
                / "NativeBinaryTools.exe"
            )
            self.assertIn(str(host_tool), dry_run)
            self.assertNotIn("tools/bin/NativeBinaryTools.exe", dry_run)

            build_ninja = (build_directory / "build.ninja").read_text(encoding="utf-8")
            self.assertNotIn("VERIFY_GLOBS", build_ninja)
            normalized_build_ninja = build_ninja.replace("\\", "/").replace("$:", ":")
            native_binary_tools_directory = self.source_dir / "tools" / "NativeBinaryTools"
            self.assertIn(
                (native_binary_tools_directory / "NativeBinaryTools.csproj").as_posix(),
                normalized_build_ninja,
            )
            self.assertNotIn(
                (native_binary_tools_directory / "Program.cs").as_posix(),
                normalized_build_ninja,
            )
            self.assertNotIn(
                (self.source_dir / "tools" / "Directory.Build.targets").as_posix(),
                normalized_build_ninja,
            )
            self.assertNotIn(
                (native_binary_tools_directory / "obj").as_posix(),
                normalized_build_ninja,
            )
            self.assertNotIn(
                (native_binary_tools_directory / "bin").as_posix(),
                normalized_build_ninja,
            )
            for generated_source in generated_sources:
                self.assertNotIn(generated_source.as_posix(), normalized_build_ninja)

    @unittest.skipUnless(sys.platform == "win32", "requires Windows Ninja semantics")
    def test_ninja_direct_build_regenerates_after_configure_input_changes(self) -> None:
        top_level_cmake = (self.source_dir / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertNotIn("CMAKE_SUPPRESS_REGENERATION", top_level_cmake)

        with tempfile.TemporaryDirectory(prefix="sandbox ninja regeneration ") as temporary_root:
            fixture_directory = Path(temporary_root) / "fixture"
            fixture_directory.mkdir()
            cmake_lists = fixture_directory / "CMakeLists.txt"
            template = fixture_directory / "configured.txt.in"
            build_directory = fixture_directory / "build"
            template.write_text("@configured_value@\n", encoding="utf-8")
            cmake_lists.write_text(
                "\n".join(
                    (
                        "cmake_minimum_required(VERSION 3.25)",
                        "project(NinjaRegenerationFixture LANGUAGES NONE)",
                        "set(configured_value before)",
                        "configure_file(configured.txt.in configured.txt @ONLY)",
                        "add_custom_target(verify_configuration ALL DEPENDS configured.txt)",
                        "",
                    )
                ),
                encoding="utf-8",
            )

            self.run_cmake("-S", str(fixture_directory), "-B", str(build_directory), "-G", "Ninja")
            self.run_cmake("--build", str(build_directory), "--target", "verify_configuration")
            configured_file = build_directory / "configured.txt"
            self.assertEqual(configured_file.read_text(encoding="utf-8"), "before\n")

            updated_cmake_lists = cmake_lists.read_text(encoding="utf-8").replace(
                "configured_value before", "configured_value after"
            )
            cmake_lists.write_text(updated_cmake_lists, encoding="utf-8")
            timestamp = cmake_lists.stat()
            os.utime(
                cmake_lists,
                ns=(timestamp.st_atime_ns, timestamp.st_mtime_ns + 1_000_000_000),
            )

            self.run_cmake("--build", str(build_directory), "--target", "verify_configuration")
            self.assertEqual(configured_file.read_text(encoding="utf-8"), "after\n")

    def create_generated_source_sentinels(self) -> tuple[Path, ...]:
        native_binary_tools_directory = self.source_dir / "tools" / "NativeBinaryTools"
        generated_sources = (
            native_binary_tools_directory / "obj" / "cmake-native-workflow-test" / "Ignored.cs",
            native_binary_tools_directory / "bin" / "cmake-native-workflow-test" / "Ignored.cs",
        )
        for generated_source in generated_sources:
            generated_source.parent.mkdir(parents=True, exist_ok=True)
            generated_source.write_text("// CMake source-discovery sentinel.\n", encoding="utf-8")

        self.addCleanup(self.remove_generated_source_sentinels, generated_sources)
        return generated_sources

    @staticmethod
    def remove_generated_source_sentinels(generated_sources: tuple[Path, ...]) -> None:
        for generated_source in generated_sources:
            generated_source.unlink(missing_ok=True)
            generated_source.parent.rmdir()

    def run_cmake(self, *arguments: str) -> str:
        result = subprocess.run(
            [self.cmake, *arguments],
            check=False,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
        )
        output = result.stdout + result.stderr
        self.assertEqual(result.returncode, 0, output)
        return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--cmake", required=True)
    arguments, unittest_arguments = parser.parse_known_args()
    NativeWorkflowTests.source_dir = arguments.source_dir.resolve()
    NativeWorkflowTests.cmake = arguments.cmake
    unittest.main(argv=[sys.argv[0], *unittest_arguments])
