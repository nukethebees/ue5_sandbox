from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


PRESET_DIRECTORY = Path(__file__).resolve().parent


class NativeWorkflowTests(unittest.TestCase):
    source_dir: Path
    cmake: str

    presets = json.loads((PRESET_DIRECTORY / "native.json").read_text(encoding="utf-8"))
    unreal_presets = json.loads(
        (PRESET_DIRECTORY / "unreal.json").read_text(encoding="utf-8")
    )

    def test_native_preset_uses_the_unreal_disabled_default(self) -> None:
        configure_presets = {
            preset["name"]: preset for preset in self.presets["configurePresets"]
        }

        native = configure_presets["native"]
        self.assertEqual(native["inherits"], "win-x64-clangcl-debug-unity")
        self.assertFalse(
            configure_presets["native-common"]["cacheVariables"]["SANDBOX_WITH_UNREAL"]
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
                "-DSANDBOX_WITH_UNREAL=OFF",
            )

            cache = (build_directory / "CMakeCache.txt").read_text(encoding="utf-8")
            self.assertIn("SANDBOX_WITH_UNREAL:BOOL=OFF", cache)

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
                (native_binary_tools_directory / "Program.cs").as_posix(),
                normalized_build_ninja,
            )
            self.assertIn(
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
