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

    def test_uses_staged_tool_without_contacting_jobserver_or_dotnet(self) -> None:
        repository = self.create_temporary_repository()
        runner = self.staged_runner(repository)
        output = self.resolve_runner(repository)

        self.assertEqual(output, str(runner))
        self.assertFalse((repository / "build-count.txt").exists())
        self.assertFalse((repository / "jobserver-count.txt").exists())

    def test_missing_tool_submits_one_shared_build_job(self) -> None:
        repository = self.create_temporary_repository()
        output = self.resolve_runner(repository)
        runner = repository / "tools" / "bin" / "BenchmarkTools.exe"

        self.assertEqual(output, str(runner))
        self.assertTrue(runner.is_file())
        self.assertFalse((repository / "build-count.txt").exists())
        self.assertEqual(
            (repository / "jobserver-count.txt").read_text(encoding="utf-8").splitlines(),
            ["submitted"],
        )
        arguments = (repository / "jobserver-arguments.txt").read_text(encoding="utf-8")
        self.assertIn("run", arguments)
        self.assertIn("--name \"Build BenchmarkTools\"", arguments)
        self.assertIn("--kind build", arguments)
        self.assertIn("--worktree", arguments)
        self.assertIn(str(repository), arguments)
        self.assertIn("--shared machine", arguments)
        self.assertIn("-File", arguments)
        self.assertIn(str(self.source_dir / "Scripts" / "BenchmarkTools.ps1"), arguments)
        self.assertIn("-RepositoryRoot", arguments)
        self.assertIn("-DotnetExecutable", arguments)
        self.assertIn(str(self.fake_dotnet), arguments)

    def test_missing_tool_inside_jobserver_builds_directly_without_recursing(self) -> None:
        repository = self.create_temporary_repository()
        output = self.resolve_runner(
            repository,
            {"NUKETHEBEES_JOBSERVER_JOB": "job-123"},
        )
        runner = repository / "tools" / "bin" / "BenchmarkTools.exe"

        self.assertEqual(output, str(runner))
        self.assertTrue(runner.is_file())
        self.assertFalse((repository / "jobserver-count.txt").exists())
        dotnet_arguments = (repository / "build-arguments.txt").read_text(encoding="utf-8")
        self.assertIn("build", dotnet_arguments)
        self.assertIn(
            str(repository / "tools" / "BenchmarkTools" / "BenchmarkTools.csproj"),
            dotnet_arguments,
        )
        self.assertIn("-m:1", dotnet_arguments)

    def test_jobserver_failure_does_not_claim_the_tool_was_staged(self) -> None:
        repository = self.create_temporary_repository()
        result = self.invoke_resolver(
            repository,
            {"BENCHMARK_TOOLS_TEST_JOBSERVER_EXIT_CODE": "17"},
        )

        self.assertEqual(result.returncode, 1)
        self.assertIn(
            "BenchmarkTools jobserver build exited with code 17.",
            result.stdout + result.stderr,
        )
        self.assertFalse((repository / "tools" / "bin" / "BenchmarkTools.exe").exists())
        self.assertFalse((repository / "build-count.txt").exists())

    def test_dotnet_failure_inside_jobserver_propagates(self) -> None:
        repository = self.create_temporary_repository()
        result = self.invoke_resolver(
            repository,
            {
                "NUKETHEBEES_JOBSERVER_JOB": "job-123",
                "BENCHMARK_TOOLS_TEST_DOTNET_EXIT_CODE": "23",
            },
        )

        self.assertEqual(result.returncode, 1)
        self.assertIn(
            "BenchmarkTools build exited with code 23.",
            result.stdout + result.stderr,
        )
        self.assertFalse((repository / "jobserver-count.txt").exists())
        self.assertFalse((repository / "tools" / "bin" / "BenchmarkTools.exe").exists())

    def test_direct_build_restores_the_previous_msbuild_node_reuse_value(self) -> None:
        repository = self.create_temporary_repository()
        result = self.invoke_resolver(
            repository,
            {
                "NUKETHEBEES_JOBSERVER_JOB": "job-123",
                "MSBUILDDISABLENODEREUSE": "previous-value",
            },
            include_node_reuse=True,
        )

        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("NODE_REUSE:previous-value", result.stdout)

    def test_wrappers_use_the_shared_resolver_and_keep_benchmark_options(self) -> None:
        fighter = (self.source_dir / "Scripts" / "run-fighter-simulation-benchmark.ps1").read_text(
            encoding="utf-8"
        )
        frame_memory = (
            self.source_dir / "Scripts" / "run-frame-memory-level-benchmark.ps1"
        ).read_text(encoding="utf-8")

        for contents in (fighter, frame_memory):
            self.assertIn(". (Join-Path $PSScriptRoot 'BenchmarkTools.ps1')", contents)
            self.assertIn("$runner = Get-BenchmarkToolsPath -RepositoryRoot $repo", contents)

        self.assertIn("'--fighter-stress-caps'", fighter)
        self.assertIn("'--warmup-seconds'", fighter)
        self.assertIn("'--saturation-timeout-seconds'", fighter)
        self.assertIn("'--game-speed'", frame_memory)
        self.assertIn("'100'", frame_memory)
        self.assertIn("'--build-preset'", frame_memory)
        self.assertIn("'frame-memory-level-benchmark'", frame_memory)

    def create_temporary_repository(self) -> Path:
        temporary_root = self.source_dir / ".local" / f"benchmark tools bootstrap {uuid.uuid4()}"
        temporary_root.mkdir(parents=True)
        self.addCleanup(shutil.rmtree, temporary_root, ignore_errors=True)
        repository = temporary_root / "native worktree with spaces"
        project = repository / "tools" / "BenchmarkTools" / "BenchmarkTools.csproj"
        project.parent.mkdir(parents=True)
        project.write_text("<Project />\n", encoding="utf-8")

        self.fake_dotnet = repository / "fake dotnet.cmd"
        self.fake_dotnet.write_text(
            "@echo off\r\n"
            "if \"%BENCHMARK_TOOLS_TEST_DOTNET_EXIT_CODE%\"==\"23\" exit /b 23\r\n"
            "echo built>> \"%BENCHMARK_TOOLS_TEST_ROOT%\\build-count.txt\"\r\n"
            "echo %*>> \"%BENCHMARK_TOOLS_TEST_ROOT%\\build-arguments.txt\"\r\n"
            "if not exist \"%BENCHMARK_TOOLS_TEST_ROOT%\\tools\\bin\" mkdir \"%BENCHMARK_TOOLS_TEST_ROOT%\\tools\\bin\"\r\n"
            "type nul > \"%BENCHMARK_TOOLS_TEST_ROOT%\\tools\\bin\\BenchmarkTools.exe\"\r\n"
            "exit /b 0\r\n",
            encoding="utf-8",
        )

        self.fake_jobserver = repository / "fake jobserver.cmd"
        self.fake_jobserver.write_text(
            "@echo off\r\n"
            "if \"%BENCHMARK_TOOLS_TEST_JOBSERVER_EXIT_CODE%\"==\"17\" exit /b 17\r\n"
            "echo submitted>> \"%BENCHMARK_TOOLS_TEST_ROOT%\\jobserver-count.txt\"\r\n"
            "echo %*>> \"%BENCHMARK_TOOLS_TEST_ROOT%\\jobserver-arguments.txt\"\r\n"
            "if not exist \"%BENCHMARK_TOOLS_TEST_ROOT%\\tools\\bin\" mkdir \"%BENCHMARK_TOOLS_TEST_ROOT%\\tools\\bin\"\r\n"
            "type nul > \"%BENCHMARK_TOOLS_TEST_ROOT%\\tools\\bin\\BenchmarkTools.exe\"\r\n"
            "exit /b 0\r\n",
            encoding="utf-8",
        )
        return repository

    def staged_runner(self, repository: Path) -> Path:
        runner = repository / "tools" / "bin" / "BenchmarkTools.exe"
        runner.parent.mkdir(parents=True)
        runner.touch()
        return runner

    def resolve_runner(self, repository: Path, overrides: dict[str, str] | None = None) -> str:
        result = self.invoke_resolver(repository, overrides)

        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result_lines = [line for line in result.stdout.splitlines() if line.startswith("RESULT:")]
        self.assertEqual(result_lines, [f"RESULT:{repository / 'tools' / 'bin' / 'BenchmarkTools.exe'}"])
        return result_lines[0].removeprefix("RESULT:")

    def invoke_resolver(
        self,
        repository: Path,
        overrides: dict[str, str] | None = None,
        *,
        include_node_reuse: bool = False,
    ) -> subprocess.CompletedProcess[str]:
        helper = self.source_dir / "Scripts" / "BenchmarkTools.ps1"
        command = " ".join(
            [
                "$ErrorActionPreference = 'Stop';",
                f". '{self.power_shell_quote(helper)}';",
                "$runner = Get-BenchmarkToolsPath",
                f"-RepositoryRoot '{self.power_shell_quote(repository)}'",
                f"-DotnetExecutable '{self.power_shell_quote(self.fake_dotnet)}'",
                f"-JobserverExecutable '{self.power_shell_quote(self.fake_jobserver)}';",
                'Write-Output "RESULT:$runner";',
                'Write-Output "NODE_REUSE:$env:MSBUILDDISABLENODEREUSE"'
                if include_node_reuse
                else "",
            ]
        )
        environment = os.environ.copy()
        environment["BENCHMARK_TOOLS_TEST_ROOT"] = str(repository)
        if overrides is not None:
            environment.update(overrides)
        return subprocess.run(
            [self.powershell, "-NoProfile", "-NonInteractive", "-Command", command],
            check=False,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            env=environment,
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
