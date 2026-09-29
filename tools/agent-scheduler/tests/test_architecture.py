"""Small boundary contracts for the admission-only replacement."""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]


class ArchitectureTests(unittest.TestCase):
    def test_codex_patch_stays_above_process_infrastructure(self):
        patch = (ROOT / "tools/agent-scheduler/codex.patch").read_text(encoding="utf-8")
        paths = set(re.findall(r"^\+\+\+ b/(.+)$", patch, re.MULTILINE))
        self.assertEqual(paths, {
            "codex-rs/Cargo.lock",
            "codex-rs/core/Cargo.toml",
            "codex-rs/core/src/tools/registry.rs",
        })

    def test_execution_modules_and_helper_client_are_deleted(self):
        for path in (
            "tools/jobserver/cli/broker.cpp",
            "tools/jobserver/lib/include/jobserver/executor.hpp",
            "tools/jobserver/lib/include/jobserver/authority.hpp",
            "tools/BenchmarkTools/JobserverExecution.cs",
            "tools/agent-scheduler/src/main.rs",
            "tools/agent-scheduler/src/invocation.rs",
            "cmake/jobserver_commands.cmake",
            "cmake/jobserver_integration.cmake",
            "cmake/unreal_jobserver.cmake",
        ):
            with self.subTest(path=path):
                self.assertFalse((ROOT / path).exists())


if __name__ == "__main__":
    unittest.main()
