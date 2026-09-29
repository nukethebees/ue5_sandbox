from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


class RustHostToolTests(unittest.TestCase):
    source_dir: Path
    cmake: str

    def test_host_build_tracks_sources_and_keeps_outputs_private(self) -> None:
        with tempfile.TemporaryDirectory(prefix="sandbox rust host ") as directory:
            root = Path(directory)
            workspace = root / "tools/rust"
            source = workspace / "src"
            source.mkdir(parents=True)
            (workspace / "Cargo.toml").write_text(
                '[package]\nname = "fixture"\nversion = "0.1.0"\nedition = "2024"\n',
                encoding="utf-8",
            )
            (workspace / "Cargo.lock").write_text(
                'version = 4\n[[package]]\nname = "fixture"\nversion = "0.1.0"\n',
                encoding="utf-8",
            )
            shutil.copy2(self.source_dir / "tools/rust/rust-toolchain.toml", workspace)
            program = source / "main.rs"
            program.write_text('fn main() { println!("before"); }\n', encoding="utf-8")
            helper = (self.source_dir / "cmake/rust_host_tools.cmake").as_posix()
            suffix = ".exe" if sys.platform == "win32" else ""
            (root / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 4.4.2)\n'
                'project(RustHostFixture LANGUAGES NONE)\n'
                f'set(CMAKE_EXECUTABLE_SUFFIX "{suffix}")\n'
                f'include("{helper}")\n'
                'sandbox_add_rust_host_tool(fixture TOOL)\n'
                'add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/consumed${CMAKE_EXECUTABLE_SUFFIX}"\n'
                '  COMMAND "${CMAKE_COMMAND}" -E copy\n'
                '  "${TOOL}" "${CMAKE_BINARY_DIR}/consumed${CMAKE_EXECUTABLE_SUFFIX}"\n'
                '  DEPENDS "${TOOL}" fixture-host VERBATIM)\n'
                'add_custom_target(consume DEPENDS "${CMAKE_BINARY_DIR}/consumed${CMAKE_EXECUTABLE_SUFFIX}")\n',
                encoding="utf-8",
            )
            build = root / "build"
            self.run_command(self.cmake, "-S", str(root), "-B", str(build), "-G", "Ninja")
            self.run_command(self.cmake, "--build", str(build), "--target", "consume")
            executable = build / "rust-tools/release" / f"fixture{suffix}"
            self.assertEqual(self.run_command(str(executable)).strip(), "before")
            self.assertTrue((build / f"consumed{suffix}").is_file())
            self.assertFalse((workspace / "target").exists())
            unchanged = executable.stat().st_mtime_ns
            self.run_command(self.cmake, "--build", str(build), "--target", "consume")
            self.assertEqual(executable.stat().st_mtime_ns, unchanged)
            (source / "message.rs").write_text('pub const TEXT: &str = "after";\n', encoding="utf-8")
            program.write_text('mod message;\nfn main() { println!("{}", message::TEXT); }\n', encoding="utf-8")
            self.run_command(self.cmake, "--build", str(build), "--target", "consume")
            self.assertEqual(self.run_command(str(executable)).strip(), "after")
            self.assertEqual(self.run_command(str(build / f"consumed{suffix}")).strip(), "after")
            executable.unlink()
            self.run_command(self.cmake, "--build", str(build), "--target", "consume")
            self.assertTrue(executable.is_file())

    def run_command(self, *arguments: str) -> str:
        result = subprocess.run(arguments, capture_output=True, text=True, errors="replace")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--cmake", required=True)
    args = parser.parse_args()
    RustHostToolTests.source_dir = args.source_dir.resolve()
    RustHostToolTests.cmake = args.cmake
    unittest.main(argv=[sys.argv[0]])
