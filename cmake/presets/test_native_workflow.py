from __future__ import annotations

import argparse
import json
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
                "--build", str(build_directory), "--target", "native-tests", "--", "-n"
            )
            self.assertNotRegex(dry_run, r"UnrealBuildTools|UnrealEditor|RunUBT")
            self.assertNotRegex(dry_run, r"(?i)(?:^|[\\/\s])unreal(?:[\\/\s]|$)")

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
            normalized_build_ninja = build_ninja.replace("\\", "/")
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
