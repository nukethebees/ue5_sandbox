from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest


class DotnetHostToolTests(unittest.TestCase):
    source_dir: Path
    cmake: str

    def test_architecture_checks_builds_with_isolated_intermediates(self) -> None:
        with tempfile.TemporaryDirectory(prefix="sandbox dotnet host tool ") as temporary_root:
            fixture_root = Path(temporary_root) / "fixture"
            tool_directory = fixture_root / "tools" / "ArchitectureChecks"
            shutil.copytree(
                self.source_dir / "tools" / "ArchitectureChecks",
                tool_directory,
                ignore=shutil.ignore_patterns("bin", "obj"),
            )
            for file_name in ("Directory.Build.props", "Directory.Build.targets"):
                shutil.copy2(self.source_dir / "tools" / file_name, fixture_root / "tools" / file_name)

            source_obj_directory = tool_directory / "obj"
            legacy_source = source_obj_directory / "Debug" / "net10.0" / "LegacyAssemblyInfo.cs"
            legacy_source.parent.mkdir(parents=True)
            legacy_source.write_text(
                "using System.Reflection;\n[assembly: AssemblyTitle(\"legacy\")]\n",
                encoding="utf-8",
            )
            hidden_source = tool_directory / ".hidden" / "LegacyAssemblyInfo.cs"
            hidden_source.parent.mkdir()
            hidden_source.write_text(
                "using System.Reflection;\n[assembly: AssemblyTitle(\"hidden\")]\n",
                encoding="utf-8",
            )

            helper_path = (self.source_dir / "cmake" / "dotnet_host_tools.cmake").as_posix()
            (fixture_root / "CMakeLists.txt").write_text(
                "\n".join(
                    [
                        "cmake_minimum_required(VERSION 4.4.2)",
                        "project(DotnetHostToolFixture LANGUAGES NONE)",
                        f'include("{helper_path}")',
                        "add_custom_target(csharp-host-tools)",
                        "sandbox_add_dotnet_host_tool(architecture-checks-host "
                        "SANDBOX_ARCHITECTURE_CHECKS "
                        '"${CMAKE_CURRENT_SOURCE_DIR}/tools/ArchitectureChecks/ArchitectureChecks.csproj")',
                        "",
                    ]
                ),
                encoding="utf-8",
            )

            build_directory = fixture_root / "build"
            self.run_cmake("-S", str(fixture_root), "-B", str(build_directory), "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug")

            executable = build_directory / "host-tools" / "ArchitectureChecks" / "Debug" / "ArchitectureChecks.exe"
            self.assertFalse((fixture_root / "tools" / "bin" / "ArchitectureChecks.exe").exists())
            self.run_cmake("--build", str(build_directory), "--target", "architecture-checks-host")
            self.assertTrue(executable.is_file())
            self.assertEqual(
                legacy_source.read_text(encoding="utf-8"),
                "using System.Reflection;\n[assembly: AssemblyTitle(\"legacy\")]\n",
            )
            self.assertEqual(
                hidden_source.read_text(encoding="utf-8"),
                "using System.Reflection;\n[assembly: AssemblyTitle(\"hidden\")]\n",
            )
            self.assertEqual(
                [path.relative_to(source_obj_directory) for path in source_obj_directory.rglob("*") if path.is_file()],
                [legacy_source.relative_to(source_obj_directory)],
            )

            build_ninja = (build_directory / "build.ninja").read_text(encoding="utf-8")
            intermediate_directory = executable.parent / "obj"
            self.assertIn(intermediate_directory.as_posix(), build_ninja.replace("\\", "/"))
            self.assertNotIn(legacy_source.as_posix(), build_ninja.replace("\\", "/"))

            original_timestamp = executable.stat().st_mtime_ns
            time.sleep(1.1)
            program = tool_directory / "Program.cs"
            program.write_text(program.read_text(encoding="utf-8") + "\n", encoding="utf-8")
            self.run_cmake("--build", str(build_directory), "--target", "architecture-checks-host")
            self.assertGreater(executable.stat().st_mtime_ns, original_timestamp)
            self.assertEqual(
                [path.relative_to(source_obj_directory) for path in source_obj_directory.rglob("*") if path.is_file()],
                [legacy_source.relative_to(source_obj_directory)],
            )

    def test_workflows_do_not_prebuild_all_host_tools(self) -> None:
        presets = json.loads(
            (self.source_dir / "cmake" / "presets" / "unreal.json").read_text(encoding="utf-8")
        )
        workflows = {preset["name"]: preset for preset in presets["workflowPresets"]}
        for workflow_name in (
            "setup-worktree-debug-game",
            "setup-worktree-development",
            "development",
            "debug-game-tests",
            "debug-game-full-tests",
        ):
            build_names = [
                step["name"]
                for step in workflows[workflow_name]["steps"]
                if step["type"] == "build"
            ]
            self.assertFalse(
                any(name.startswith("csharp-host-tools") for name in build_names),
                workflow_name,
            )

    def run_cmake(self, *arguments: str) -> None:
        result = subprocess.run(
            [self.cmake, *arguments],
            check=False,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--cmake", required=True)
    arguments = parser.parse_args()
    DotnetHostToolTests.source_dir = arguments.source_dir.resolve()
    DotnetHostToolTests.cmake = arguments.cmake
    unittest.main(argv=[sys.argv[0]])
