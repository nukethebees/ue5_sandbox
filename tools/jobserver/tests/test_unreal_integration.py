"""Exercise production Unreal CMake wrappers without loading or building Unreal."""
from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
from typing import Any, TextIO, cast, override
import unittest
import uuid

SOURCE_ROOT, CLIENT, DAEMON, HELPER, CMAKE, CTEST = map(Path, sys.argv[1:7])
Fixture = tuple[Path, Path, Path]


class UnrealIntegrationTests(unittest.TestCase):
    @override
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="jobserver-unreal-")
        self.root = Path(self.temporary.name)
        self.environment = {key: value for key, value in os.environ.items()
                            if not key.startswith("NUKETHEBEES_JOBSERVER_")}
        self.environment.update({
            "NUKETHEBEES_JOBSERVER_TEST_PIPE": rf"\\.\pipe\NukeTheBees.UnrealFixture.{uuid.uuid4().hex}",
            "NUKETHEBEES_JOBSERVER_TEST_DATA": str(self.root / "state"),
        })
        self.processes: list[subprocess.Popen[str]] = []
        self.outputs: list[TextIO] = []
        self.junctions: list[Path] = []
        self.addCleanup(self.cleanup)
        self.daemon = self.start([str(DAEMON)])
        self.status()
        self.engine = self.root / "engine checkout α"
        self.engine.mkdir()
        self.fixture_index = 0

    def cleanup(self) -> None:
        if hasattr(self, "daemon") and self.daemon.poll() is None:
            try:
                self.checked([str(CLIENT), "shutdown"])
            except (subprocess.SubprocessError, AssertionError):
                self.daemon.kill()
        for process in self.processes:
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
        for output in self.outputs:
            output.close()
        for junction in self.junctions:
            junction.rmdir()
        self.temporary.cleanup()

    def checked(self, command: list[str], *, cwd: Path | None = None) -> str:
        result = subprocess.run(command, cwd=cwd, env=self.environment, capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=15,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout

    def start(self, command: list[str], *, environment: dict[str, str] | None = None
              ) -> subprocess.Popen[str]:
        output = cast(TextIO, tempfile.TemporaryFile(mode="w+", encoding="utf-8"))
        self.outputs.append(output)
        process = subprocess.Popen(command, env=environment or self.environment,
                                   stdout=output, stderr=subprocess.STDOUT, text=True,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        self.processes.append(process)
        return process

    def status(self) -> dict[str, Any]:
        return cast(dict[str, Any], json.loads(self.checked([str(CLIENT), "status", "--json"])))

    def configure(self, engine: str | Path | None = None) -> Fixture:
        self.fixture_index += 1
        source = self.root / f"worktree {self.fixture_index} α"
        shutil.copytree(SOURCE_ROOT / "tools/jobserver/tests/unreal_fixture", source)
        build, ready = source / "build", source / "ready.txt"
        self.checked([str(CMAKE), "-S", str(source), "-B", str(build), "-G", "Ninja",
                      f"-DSOURCE_ROOT={SOURCE_ROOT.as_posix()}", f"-DCLIENT={CLIENT.as_posix()}",
                      f"-DHELPER={HELPER.as_posix()}", f"-DENGINE_ROOT={engine or self.engine}",
                      f"-DREADY={ready.as_posix()}"])
        return source, build, ready

    def launch(self, fixture: Fixture, target: str = "reader") -> subprocess.Popen[str]:
        return self.start([str(CMAKE), "--build", str(fixture[1]), "--target", target])

    def wait_ready(self, path: Path) -> None:
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if path.exists() and path.read_text().strip():
                return
            time.sleep(0.01)
        self.fail(f"Process never became ready: {path}; status={self.status()}")

    def wait_job(self, fixture: Fixture, state: str) -> dict[str, Any]:
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            for job in self.status()["jobs"]:
                if Path(job["worktree"]) == fixture[0] and job["state"] == state:
                    return cast(dict[str, Any], job)
            time.sleep(0.01)
        self.fail(f"No {state} job for {fixture[0]}; status={self.status()}")

    def cancel(self, fixture: Fixture) -> None:
        job = self.wait_job(fixture, "RUNNING")
        self.checked([str(CLIENT), "kill", job["id"]])

    def test_editor_test_reader_blocks_writer(self) -> None:
        reader, writer = self.configure(), self.configure()
        self.start([str(CTEST), "--test-dir", str(reader[1]), "-R", "^reader-test$",
                    "--output-on-failure"])
        self.wait_ready(reader[2])
        self.launch(writer, "writer")
        self.wait_job(writer, "QUEUED")
        self.assertFalse(writer[2].exists())
        self.cancel(reader)
        self.wait_ready(writer[2])

    def test_writer_blocks_reader(self) -> None:
        writer, reader = self.configure(), self.configure()
        self.launch(writer, "writer")
        self.wait_ready(writer[2])
        self.launch(reader)
        self.wait_job(reader, "QUEUED")
        self.assertFalse(reader[2].exists())
        self.cancel(writer)
        self.wait_ready(reader[2])

    def test_readers_overlap_and_other_engine_writer_is_independent(self) -> None:
        first, second = self.configure(), self.configure()
        other_engine = self.root / "other engine"
        other_engine.mkdir()
        other = self.configure(other_engine)
        for fixture, target in ((first, "reader"), (second, "reader"), (other, "writer")):
            self.launch(fixture, target)
            self.wait_ready(fixture[2])
            self.wait_job(fixture, "RUNNING")

    def test_queued_writer_prevents_late_reader_starvation(self) -> None:
        older, writer, later = self.configure(), self.configure(), self.configure()
        self.launch(older)
        self.wait_ready(older[2])
        self.launch(writer, "writer")
        self.wait_job(writer, "QUEUED")
        self.launch(later)
        self.wait_job(later, "QUEUED")
        self.cancel(older)
        self.wait_ready(writer[2])
        self.wait_job(later, "QUEUED")
        self.assertFalse(later[2].exists())
        self.cancel(writer)
        self.wait_ready(later[2])

    def test_native_work_does_not_wait_for_engine_writer(self) -> None:
        writer, native = self.configure(), self.configure()
        self.launch(writer, "writer")
        self.wait_ready(writer[2])
        self.launch(native, "native")
        self.wait_ready(native[2])
        self.wait_job(writer, "RUNNING")

    def test_canonical_identity_and_generated_claims(self) -> None:
        original = self.configure()
        resource = (original[1] / "resource.txt").read_text()
        alias = self.root / "engine junction"
        self.checked(["cmd", "/c", "mklink", "/J", str(alias), str(self.engine)])
        self.junctions.append(alias)
        for variant in (self.engine.as_posix(), str(self.engine).upper(), str(self.engine) + "\\",
                        str(self.engine) + "\\.", str(alias)):
            with self.subTest(engine=variant):
                fixture = self.configure(variant)
                self.assertEqual((fixture[1] / "resource.txt").read_text(), resource)
        writer, reader, benchmark = (original[1] / "claims.txt").read_text().splitlines()
        self.assertIn(f"--exclusive;{resource};--", writer)
        self.assertNotIn("--resource;", writer)
        self.assertIn(f"--shared;{resource};--", reader)
        self.assertIn("--exclusive;machine;--exclusive;benchmark", benchmark)
        self.assertIn(f"--shared;{resource};--", benchmark)
        for filename in ("build.ninja", "CTestTestfile.cmake"):
            generated = (original[1] / filename).read_text(encoding="utf-8")
            self.assertIn(f"--shared {resource}", generated.replace('"', ''))
        ninja = (original[1] / "build.ninja").read_text(encoding="utf-8").replace("\\", "/")
        for target in ("commandlet", "benchmark-commandlet"):
            section = ninja.split(f"# Custom command for CMakeFiles/{target}\n", 1)[1].split("\n\n", 1)[0]
            self.assertIn(f"--shared {resource}", section)
            if target == "benchmark-commandlet":
                self.assertIn("--exclusive machine --exclusive benchmark", section)
        for target in ("cook", "cook-incremental", "stage", "archive"):
            section = ninja.split(f"# Custom command for CMakeFiles/{target}\n", 1)[1].split("\n\n", 1)[0]
            self.assertIn(f"--exclusive {resource}", section)
        verification = ninja.split("# Custom command for CMakeFiles/verify-package\n", 1)[1].split("\n\n", 1)[0]
        self.assertIn(f"--shared {resource}", verification)
        staged = ninja.split("# Custom command for CMakeFiles/run-staged\n", 1)[1].split("\n\n", 1)[0]
        self.assertIn("--shared machine", staged)
        self.assertNotIn(resource, staged)

    def test_powershell_workflow_does_not_reserve_parent_engine(self) -> None:
        fixture = self.configure()
        environment = self.environment | {
            "FIXTURE_SOURCE_ROOT": SOURCE_ROOT.as_posix(), "FIXTURE_CLIENT": CLIENT.as_posix(),
            "FIXTURE_HELPER": HELPER.as_posix(), "FIXTURE_ENGINE": self.engine.as_posix(),
            "FIXTURE_READY": fixture[2].as_posix(), "FIXTURE_BUILD": fixture[1].as_posix(),
        }
        self.start(["pwsh", "-NoProfile", "-File", str(fixture[0] / "workflow.ps1"),
                    "-SourceRoot", str(SOURCE_ROOT)], environment=environment)
        self.wait_ready(fixture[2])
        self.assertEqual(self.wait_job(fixture, "RUNNING")["kind"], "unreal-build")
        self.assertEqual(len(self.status()["jobs"]), 1)

    def test_exclusive_parent_covers_nested_reader(self) -> None:
        fixture = self.configure()
        resource = (fixture[1] / "resource.txt").read_text()
        self.start([str(CLIENT), "run", "--name", "parent", "--worktree", str(fixture[0]),
                    "--shared", "machine", "--exclusive", resource, "--", str(CMAKE),
                    "--build", str(fixture[1]), "--target", "reader"])
        self.wait_ready(fixture[2])
        self.assertEqual(len(self.status()["jobs"]), 1)
        self.cancel(fixture)

    def test_shared_parent_cannot_upgrade_to_nested_writer(self) -> None:
        fixture = self.configure()
        resource = (fixture[1] / "resource.txt").read_text()
        result = subprocess.run(
            [str(CLIENT), "run", "--shared", "machine", "--shared", resource, "--", str(CMAKE),
             "--build", str(fixture[1]), "--target", "writer"],
            env=self.environment, capture_output=True, text=True, encoding="utf-8", errors="replace",
            timeout=10, creationflags=subprocess.CREATE_NO_WINDOW)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("nested_resource_not_held", result.stdout + result.stderr)
        self.assertFalse(fixture[2].exists())

    def test_descendant_retains_reader_gate_after_root_exit(self) -> None:
        reader, writer = self.configure(), self.configure()
        self.launch(reader, "descendant-reader")
        self.wait_ready(Path(str(reader[2]) + ".child"))
        self.launch(writer, "writer")
        self.wait_job(writer, "QUEUED")
        self.assertFalse(writer[2].exists())
        self.cancel(reader)
        self.wait_ready(writer[2])


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
