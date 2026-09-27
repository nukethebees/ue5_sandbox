from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


class CentralToolsTests(unittest.TestCase):
    source: Path

    def run_command(self, arguments: list[str], *, success: bool = True, env: dict[str, str] | None = None) -> str:
        result = subprocess.run(arguments, capture_output=True, text=True, env=env)
        output = result.stdout + result.stderr
        if success:
            self.assertEqual(result.returncode, 0, output)
        else:
            self.assertNotEqual(result.returncode, 0, output)
        return output

    def test_private_install_upgrade_and_failed_validation(self) -> None:
        with tempfile.TemporaryDirectory(prefix="central tools ", dir=self.source / ".local") as temporary:
            root = Path(temporary)
            failing_dotnet = root / "fail tests.cmd"
            failing_dotnet.write_text(
                '@echo off\nif "%1"=="test" exit /b 23\n'
                f'"{shutil.which("dotnet")}" %*\n', encoding="utf-8"
            )
            for tool in ("UnrealBuildTools", "CodeFormatTools"):
                with self.subTest(tool=tool):
                    install_root = root / tool
                    command = ["pwsh", "-NoProfile", "-File",
                               str(self.source / "tools/install/Install-CentralDotnetTool.ps1"),
                               "-ToolName", tool, "-InstallRoot", str(install_root)]
                    self.run_command(command)
                    installed = install_root / "bin"
                    executable = installed / f"{tool}.exe"
                    self.assertIn(f"{tool} 1.0.0", self.run_command([str(executable), "--version"]))
                    self.assertTrue((installed / f"{tool}.runtimeconfig.json").is_file())
                    self.assertTrue((installed / f"{tool}.deps.json").is_file())
                    before = self.hashes(installed)
                    failure = self.run_command(command + ["-DotnetExecutable", str(failing_dotnet)], success=False)
                    self.assertIn("candidate tests failed (23)", failure)
                    self.assertEqual(self.hashes(installed), before)
                    self.assertEqual(list(install_root.iterdir()), [installed])
                    (installed / "obsolete.dll").write_text("old runtime file")
                    self.run_command(command)
                    self.assertFalse((installed / "obsolete.dll").exists())
                    self.assertEqual(list(install_root.iterdir()), [installed])
                    self.check_cmake_invocation(root, tool, installed)

    @staticmethod
    def hashes(directory: Path) -> dict[Path, str]:
        return {p.relative_to(directory): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in directory.rglob("*") if p.is_file()}

    def check_cmake_invocation(self, root: Path, tool: str, installed: Path) -> None:
        cmake = shutil.which("cmake")
        assert cmake is not None
        fixture = root / f"{tool} fixture"
        fixture.mkdir()
        lines = [
            "cmake_minimum_required(VERSION 4.4.2)",
            "project(CentralFixture LANGUAGES NONE)",
            f'include("{self.source.as_posix()}/cmake/central_tools.cmake")',
            f"sandbox_central_tool_command(command {tool})",
            "add_custom_target(version COMMAND ${command} --version VERBATIM)",
        ]
        if tool == "UnrealBuildTools":
            (fixture / "project.uproject").write_text("{}")
            (fixture / "build.cmd").write_text('@echo off\necho %* > "%~dp0arguments.txt"\nexit /b 0\n')
            lines += [
                "function(sandbox_unreal_build_jobserver_command output operation)",
                '  set(${output} "" PARENT_SCOPE)',
                "endfunction()",
                "set(IOJ_UNREAL_BUILD_TOOLS ${command})",
                f'set(UE_BUILD_SCRIPT "{fixture.as_posix()}/build.cmd")',
                f'set(IOJ_UPROJECT "{fixture.as_posix()}/project.uproject")',
                "set(UE_PLATFORM Win64)", "set(UE_CONFIGURATION Development)",
                "set(IOJ_NATIVE_TOOLCHAIN fixture)",
                f'include("{self.source.as_posix()}/cmake/unreal.cmake")',
                "add_unreal_target(editor SandboxEditor)",
            ]
        (fixture / "CMakeLists.txt").write_text("\n".join(lines))
        build = fixture / "build"
        self.run_command([cmake, "-S", str(fixture), "-B", str(build), "-G", "Ninja"])
        environment = os.environ.copy()
        environment["PATH"] = str(installed) + os.pathsep + environment["PATH"]
        self.assertIn(f"{tool} 1.0.0", self.run_command(
            [cmake, "--build", str(build), "--target", "version"], env=environment))
        if tool == "UnrealBuildTools":
            self.run_command([cmake, "--build", str(build), "--target", "editor"], env=environment)
            self.assertIn("SandboxEditor", (fixture / "arguments.txt").read_text())
        environment["PATH"] = str(root / "missing")
        failure = self.run_command([cmake, "--build", str(build), "--target", "version"],
                                   env=environment, success=False)
        self.assertIn(f"{tool} is missing from PATH", failure)
        ninja = (build / "build.ninja").read_text()
        self.assertNotIn("unreal-build-tools-host", ninja)
        self.assertNotIn("code-format-tools-host", ninja)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", type=Path, required=True)
    args = parser.parse_args()
    CentralToolsTests.source = args.source_dir.resolve()
    unittest.main(argv=[sys.argv[0]])
