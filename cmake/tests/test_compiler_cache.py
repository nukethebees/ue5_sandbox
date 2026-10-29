import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class CompilerCacheTests(unittest.TestCase):
    def test_pch_cache_reuse_and_in_place_compiler_replacement(self):
        with tempfile.TemporaryDirectory(prefix="pch-cache-") as directory:
            root = Path(directory)
            compiler = root / Path(self.compiler).name
            shutil.copy2(self.compiler, compiler)
            (root / "ccache.conf").write_text("")
            environment = os.environ.copy()
            environment.pop("CCACHE_DISABLE", None)
            environment["CCACHE_DIR"] = str(root / "cache")
            environment["CCACHE_CONFIGPATH"] = str(root / "ccache.conf")
            environment["CCACHE_LOGFILE"] = str(root / "cache.log")

            def run(*command):
                result = subprocess.run(command, env=environment, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                return result.stdout + result.stderr

            (root / "pch.hpp").write_text("#pragma once\nstruct Value { int number; };\n")
            (root / "probe.cpp").write_text("int read(Value const& value) { return value.number; }\n")
            module = self.source_dir / "cmake/compiler_cache.cmake"
            (root / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 4.4.2)\n'
                'project(PchCache LANGUAGES CXX)\n'
                f'include("{module.as_posix()}")\n'
                'add_library(probe OBJECT probe.cpp)\n'
                'target_precompile_headers(probe PRIVATE pch.hpp)\n'
            )
            build = root / "build"
            run(self.cmake, "-S", str(root), "-B", str(build), "-G", "Ninja",
                f"-DCMAKE_CXX_COMPILER={compiler}", f"-DCMAKE_AR={self.archiver}",
                "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
                "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache;compiler_check=content;sloppiness=pch_defines,time_macros")
            run(self.cmake, "--build", str(build))
            pch = next(build.rglob("*.pch"))
            timestamp = pch.stat().st_mtime_ns
            run(self.cmake, "--build", str(build))
            self.assertEqual(pch.stat().st_mtime_ns, timestamp)

            log = root / "cache.log"
            offset = log.stat().st_size
            run(self.cmake, "--build", str(build), "--target", "clean")
            run(self.cmake, "--build", str(build))
            cached = log.read_text()[offset:]
            hits = cached.count("Result: direct_cache_hit") + cached.count("Result: preprocessed_cache_hit")
            self.assertEqual(hits, 2, cached)

            old_hash = hashlib.sha256(compiler.read_bytes()).hexdigest()
            version = run(str(compiler), "--version")
            with compiler.open("ab") as output:
                output.write(b"\0IOJ compiler identity regression\0")
            new_hash = hashlib.sha256(compiler.read_bytes()).hexdigest()
            self.assertNotEqual(old_hash, new_hash)
            self.assertEqual(run(str(compiler), "--version"), version)
            offset = log.stat().st_size
            # Build without an explicit configure or clean after replacing the compiler.
            run(self.cmake, "--build", str(build))
            rules = (build / "build.ninja").read_text()
            self.assertIn(f"IOJ_PCH_COMPILER_SHA256={new_hash}", rules)
            self.assertNotIn(f"IOJ_PCH_COMPILER_SHA256={old_hash}", rules)
            self.assertGreater(pch.stat().st_mtime_ns, timestamp)
            replaced = log.read_text()[offset:]
            self.assertEqual(replaced.count("Result: cache_miss"), 2, replaced)
            self.assertNotIn("Result: direct_cache_hit", replaced)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--archiver", required=True)
    arguments = parser.parse_args()
    CompilerCacheTests.source_dir = arguments.source_dir.resolve()
    CompilerCacheTests.cmake = arguments.cmake
    CompilerCacheTests.compiler = arguments.compiler
    CompilerCacheTests.archiver = arguments.archiver
    unittest.main(argv=[__file__])
