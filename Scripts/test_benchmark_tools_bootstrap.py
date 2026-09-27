from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import unittest
import uuid


class BenchmarkToolsBootstrapTests(unittest.TestCase):
    source_dir: Path
    powershell: str

    def test_builds_configuration_local_host_even_if_output_exists(self) -> None:
        repository = self.create_temporary_repository()
        for _ in range(2):
            result = self.invoke_resolver(repository)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn(str(self.runner(repository)), result.stdout)
        commands = (repository / "commands.txt").read_text().splitlines()
        self.assertEqual(commands, ["--preset native", "--build --preset native --target benchmark-tools-host"] * 2)
        self.assertFalse((repository / "tools" / "bin").exists())

    def test_configure_failure_stops_before_build(self) -> None:
        repository = self.create_temporary_repository()
        result = self.invoke_resolver(repository, {"BENCHMARK_FAIL_CONFIGURE": "1"})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("configure failed (17)", result.stdout + result.stderr)
        self.assertFalse(self.runner(repository).exists())

    def test_build_failure_does_not_return_stale_output(self) -> None:
        repository = self.create_temporary_repository()
        self.runner(repository).parent.mkdir(parents=True)
        self.runner(repository).touch()
        result = self.invoke_resolver(repository, {"BENCHMARK_FAIL_BUILD": "1"})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("build failed (23)", result.stdout + result.stderr)

    def test_wrappers_only_bootstrap_and_forward_to_benchmark_tools(self) -> None:
        fighter = (self.source_dir / "Scripts" / "run-fighter-simulation-benchmark.ps1").read_text(
            encoding="utf-8"
        )
        frame_memory = (
            self.source_dir / "Scripts" / "run-frame-memory-level-benchmark.ps1"
        ).read_text(encoding="utf-8")

        for contents in (fighter, frame_memory):
            self.assertIn(". (Join-Path $PSScriptRoot 'BenchmarkTools.ps1')", contents)
            self.assertIn("$runner = Get-BenchmarkToolsPath -RepositoryRoot $repo", contents)

        self.assertIn("'fighter-simulation'", fighter)
        self.assertIn("'--fighter-caps'", fighter)
        self.assertIn("'--warmup-seconds'", fighter)
        self.assertIn("'--saturation-timeout-seconds'", fighter)
        self.assertIn("'frame-memory-level'", frame_memory)
        self.assertNotIn("ConvertFrom-Json", fighter)
        self.assertNotIn("ConvertFrom-Json", frame_memory)

    def create_temporary_repository(self) -> Path:
        temporary_root = self.source_dir / ".local" / f"benchmark tools bootstrap {uuid.uuid4()}"
        temporary_root.mkdir(parents=True)
        self.addCleanup(shutil.rmtree, temporary_root)
        self.fake_cmake = temporary_root / "fake cmake.cmd"
        self.fake_cmake.write_text(
            "@echo off\n"
            'echo %*>> "%BENCHMARK_ROOT%\\commands.txt"\n'
            'if "%BENCHMARK_FAIL_CONFIGURE%"=="1" exit /b 17\n'
            'if "%1"=="--preset" exit /b 0\n'
            'if "%BENCHMARK_FAIL_BUILD%"=="1" exit /b 23\n'
            'if not exist "%BENCHMARK_OUTPUT%" mkdir "%BENCHMARK_OUTPUT%"\n'
            'type nul > "%BENCHMARK_OUTPUT%\\BenchmarkTools.exe"\n',
            encoding="utf-8",
        )
        return temporary_root

    @staticmethod
    def runner(repository: Path) -> Path:
        return repository / "out/build/native/host-tools/BenchmarkTools/Debug/BenchmarkTools.exe"

    def invoke_resolver(self, repository: Path, overrides: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
        helper = self.source_dir / "Scripts" / "BenchmarkTools.ps1"
        command = (
            "$ErrorActionPreference = 'Stop'; "
            f". '{self.power_shell_quote(helper)}'; "
            f"Get-BenchmarkToolsPath -RepositoryRoot '{self.power_shell_quote(repository)}' "
            f"-CMakeExecutable '{self.power_shell_quote(self.fake_cmake)}'"
        )
        environment = os.environ.copy()
        environment.update(BENCHMARK_ROOT=str(repository), BENCHMARK_OUTPUT=str(self.runner(repository).parent))
        environment.update(overrides or {})
        return subprocess.run(
            [self.powershell, "-NoProfile", "-NonInteractive", "-Command", command],
            capture_output=True, encoding="utf-8", errors="replace", env=environment,
        )

    @staticmethod
    def power_shell_quote(path: Path) -> str:
        return str(path).replace("'", "''")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--powershell", required=True)
    arguments, unittest_arguments = parser.parse_known_args()
    BenchmarkToolsBootstrapTests.source_dir = arguments.source_dir.resolve()
    BenchmarkToolsBootstrapTests.powershell = arguments.powershell
    unittest.main(argv=[sys.argv[0], *unittest_arguments])
