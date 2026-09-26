import json
from pathlib import Path
from copy import deepcopy
import subprocess
import sys
import tempfile
from typing import Any
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

import csharp_tests


ROOT = Path(__file__).resolve().parents[1]


class CSharpTests(unittest.TestCase):
    def test_registration_matches_solution_and_explicit_ownership(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory)
            (fixture / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 4.4)\nproject(Registration NONE)\nenable_testing()\n'
                f'set(PROJECT_SOURCE_DIR "{ROOT.as_posix()}")\n'
                f'set(Python3_EXECUTABLE "{Path(sys.executable).as_posix()}")\n'
                'set(CMAKE_BUILD_TYPE Debug)\n'
                f'include("{ROOT.as_posix()}/cmake/csharp_tests.cmake")\n',
                encoding="utf-8",
            )
            build = fixture / "build"
            subprocess.run(["cmake", "-S", str(fixture), "-B", str(build), "-G", "Ninja"], check=True, capture_output=True)
            metadata = json.loads((build / "csharp-tests.json").read_text())
            expected = {
                "tools/" + project.attrib["Path"]
                for project in ET.parse(ROOT / "tools/Tools.slnx").getroot()
                if project.attrib["Path"].endswith(".Tests.csproj")
            }
            self.assertEqual({project["project"] for project in metadata["projects"]}, expected)
            csharp_tests.validate_registration(metadata)
            incomplete = dict(metadata, projects=metadata["projects"][:-1])
            with self.assertRaisesRegex(ValueError, "solution/registration mismatch"):
                csharp_tests.validate_registration(incomplete)
            missing_consumer = dict(metadata, projects=[dict(project) for project in metadata["projects"]])
            missing_consumer["projects"][0]["inputs"] = ["tools/AgentGit.Tests"]
            with self.assertRaisesRegex(ValueError, "freshness inputs"):
                csharp_tests.validate_registration(missing_consumer)
            manifest = json.loads((ROOT / ".integration-gates.json").read_text())
            # Compare semantics to the actual transitive MSBuild graph, not another table.
            for field, value in (
                ("testProjects", ["tools/GitTools.Tests/GitTools.Tests.csproj"]),
                ("affects", ["git-tools"]),
                ("paths", ["tools/misspelled/"]),
                ("gates", []),
            ):
                wrong = deepcopy(manifest)
                component = next(item for item in wrong["components"] if item["name"] == "code-format-tools")
                component[field] = value
                with self.subTest(field=field), patch.object(csharp_tests.json, "loads", return_value=wrong):
                    with self.assertRaises(ValueError):
                        csharp_tests.validate_registration(metadata)
            wrong = deepcopy(manifest)
            next(item for item in wrong["components"] if item["name"] == "git-support")["affects"] = ["agent-git"]
            with patch.object(csharp_tests.json, "loads", return_value=wrong):
                with self.assertRaisesRegex(ValueError, "Manifest consumers for tools/GitSupport/"):
                    csharp_tests.validate_registration(metadata)
            wrong = deepcopy(manifest)
            next(item for item in wrong["components"] if item["name"] == "tool-routing")["affects"] = ["code-format-tools"]
            with patch.object(csharp_tests.json, "loads", return_value=wrong):
                with self.assertRaisesRegex(ValueError, "Manifest consumers for tools/AgentGit/"):
                    csharp_tests.validate_registration(metadata)
            inventory = json.loads(subprocess.check_output(
                ["ctest", "--test-dir", str(build), "--show-only=json-v1"], text=True))
            self.assertEqual(len(expected), len(inventory["tests"]))
            for test in inventory["tests"]:
                labels = next(prop["value"] for prop in test["properties"] if prop["name"] == "LABELS")
                self.assertIn("csharp", labels)
                self.assertIn("developer-tool", labels)
                self.assertIn("test", test["command"])
                self.assertNotIn("Sandbox.CSharpTools", test["name"])
            # Shared project references must be included transitively in freshness inputs.
            for project in metadata["projects"]:
                pending = [project["project"]]
                while pending:
                    path = Path(pending.pop())
                    self.assertIn(path.parent.as_posix(), project["inputs"])
                    for reference in ET.parse(ROOT / path).iter("ProjectReference"):
                        dependency = (ROOT / path.parent / reference.attrib["Include"]).resolve().relative_to(ROOT)
                        pending.append(dependency.as_posix())

    def test_fingerprint_detects_edits_additions_deletions_and_shared_inputs(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            files = ["tools/Directory.Build.props", "tools/Directory.Build.targets", "tools/Tools.slnx",
                     ".integration-gates.json", "cmake/csharp_tests.cmake", "cmake/csharp_tests.py", "tools/Example/Source.cs"]
            for name in files:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("initial")
            metadata = {"root": str(root)}
            project = {"inputs": ["tools/Example"]}
            with patch.object(csharp_tests, "__file__", str(root / "cmake/csharp_tests.py")):
                initial = csharp_tests.fingerprint(metadata, project)
                for name in ("tools/Example/Source.cs", "tools/Directory.Build.props", ".integration-gates.json"):
                    (root / name).write_text("changed")
                    self.assertNotEqual(initial, csharp_tests.fingerprint(metadata, project))
                    (root / name).write_text("initial")
                extra = root / "tools/Example/Added.cs"
                extra.write_text("new")
                self.assertNotEqual(initial, csharp_tests.fingerprint(metadata, project))
                extra.unlink()
                self.assertEqual(initial, csharp_tests.fingerprint(metadata, project))
                (root / "tools/Example/Source.cs").unlink()
                self.assertNotEqual(initial, csharp_tests.fingerprint(metadata, project))

    def test_no_build_command_and_stale_binary_rejection(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            metadata: dict[str, Any] = {
                "root": str(root), "artifacts": str(root), "dotnet": "dotnet",
                "configuration": "Debug",
                "projects": [{"name": "Example", "project": "Example.csproj", "inputs": []}],
            }
            metadata_path = root / "metadata.json"
            metadata_path.write_text(json.dumps(metadata))
            with patch.object(sys, "argv", ["runner", "test", str(metadata_path), "Example"]), \
                    patch.object(csharp_tests, "fingerprint", return_value="current"), \
                    patch.object(csharp_tests.subprocess, "run") as run:
                self.assertEqual(1, csharp_tests.main())
                run.assert_not_called()
                (root / "Example.fingerprint").write_text("current")
                run.return_value.returncode = 0
                self.assertEqual(0, csharp_tests.main())
                arguments = run.call_args.args[0]
                self.assertIn("--no-build", arguments)
                self.assertIn("--no-restore", arguments)
                self.assertNotIn("build", arguments)


if __name__ == "__main__":
    unittest.main()
