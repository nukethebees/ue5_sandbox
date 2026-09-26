"""Run already-built C# tests, refusing stale binaries without evaluating MSBuild."""

import hashlib
import json
from pathlib import Path
import subprocess
import sys
from typing import Any
import xml.etree.ElementTree as ET


def validate_registration(metadata: dict[str, Any]) -> None:
    root = Path(metadata["root"])
    expected = {
        "tools/" + project.attrib["Path"]
        for project in ET.parse(root / "tools/Tools.slnx").getroot()
        if project.attrib["Path"].endswith(".Tests.csproj")
    }
    registered = {project["project"] for project in metadata["projects"]}
    if expected != registered:
        raise ValueError(f"Update cmake/csharp_tests.cmake: solution/registration mismatch: {sorted(expected ^ registered)}")
    consumers: dict[str, set[str]] = {}
    for project in metadata["projects"]:
        pending = [project["project"]]
        visited: set[str] = set()
        while pending:
            path = pending.pop()
            if path in visited:
                continue
            visited.add(path)
            relative = Path(path)
            consumers.setdefault(relative.parent.as_posix() + "/", set()).add(project["project"])
            if relative.parent.as_posix() not in project["inputs"]:
                raise ValueError(f"Update {project['name']} freshness inputs in cmake/csharp_tests.cmake: missing {path}")
            for reference in ET.parse(root / relative).iter("ProjectReference"):
                dependency = (root / relative.parent / reference.attrib["Include"]).resolve().relative_to(root)
                pending.append(dependency.as_posix())
    manifest = json.loads((root / ".integration-gates.json").read_text(encoding="utf-8"))
    validate_ownership(manifest, registered, consumers)


def validate_ownership(manifest: dict[str, Any], registered: set[str], consumers: dict[str, set[str]]) -> None:
    components = {component["name"]: component for component in manifest["components"]}
    declared = {project for component in components.values() for project in component.get("testProjects", [])}
    if declared != registered:
        raise ValueError(f"Manifest/registration mismatch: {sorted(declared ^ registered)}")
    for component in components.values():
        for project in component.get("testProjects", []):
            expected_gate = "agent-git-tests" if project == "tools/AgentGit.Tests/AgentGit.Tests.csproj" else "csharp-tools-tests"
            if expected_gate not in component["gates"]:
                raise ValueError(f"Missing {expected_gate} for {project}")
        for affected in component["affects"]:
            if affected not in components:
                raise ValueError(f"Unknown affected component: {affected}")
    owned_paths = {directory + "source.cs": expected for directory, expected in consumers.items()}
    for directory, expected in consumers.items():
        for component in components.values():
            for path in component["paths"]:
                if path.startswith(directory) and not path.endswith("/"):
                    owned_paths[path] = expected
    for path, expected in owned_paths.items():
        matches = [(len(prefix), component) for component in components.values() for prefix in component["paths"]
                   if path == prefix or (prefix.endswith("/") and path.startswith(prefix))]
        longest = max((length for length, _ in matches), default=-1)
        pending = [component["name"] for length, component in matches if length == longest]
        visited: set[str] = set()
        selected: set[str] = set()
        while pending:
            name = pending.pop()
            if name in visited:
                continue
            visited.add(name)
            selected.update(components[name].get("testProjects", []))
            pending.extend(components[name]["affects"])
        if selected != expected:
            raise ValueError(f"Manifest consumers for {path}: expected {sorted(expected)}, selected {sorted(selected)}")


def fingerprint(metadata: dict[str, Any], project: dict[str, Any]) -> str:
    root = Path(metadata["root"])
    files = {
        root / "tools/Directory.Build.props",
        root / "tools/Directory.Build.targets",
        root / "tools/Tools.slnx",
        root / "cmake/csharp_tests.cmake",
        root / ".integration-gates.json",
        Path(__file__),
    }
    for directory in project["inputs"]:
        source_root = root / directory
        files.update(
            path for path in source_root.rglob("*")
            if path.is_file() and not {"bin", "obj"}.intersection(path.relative_to(source_root).parts)
        )
    digest = hashlib.sha256(json.dumps(metadata, sort_keys=True).encode())
    for path in sorted(files):
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(path.read_bytes())
    return digest.hexdigest()


def main() -> int:
    action, metadata_path, *names = sys.argv[1:]
    metadata = json.loads(Path(metadata_path).read_text(encoding="utf-8"))
    if action == "validate":
        validate_registration(metadata)
        return 0
    for project in metadata["projects"]:
        if names and project["name"] not in names:
            continue
        stamp = Path(metadata["artifacts"]) / f"{project['name']}.fingerprint"
        current = fingerprint(metadata, project)
        if action == "record":
            stamp.parent.mkdir(parents=True, exist_ok=True)
            stamp.write_text(current, encoding="utf-8")
            continue
        if not stamp.exists() or stamp.read_text(encoding="utf-8") != current:
            print(f"C# tests are stale. Rebuild csharp-{project['name']}-build in this CMake build directory.", file=sys.stderr)
            return 1
        result = subprocess.run(
            [metadata["dotnet"], "test", str(Path(metadata["root"]) / project["project"]),
             "--configuration", metadata["configuration"], "--artifacts-path", metadata["artifacts"],
             "--no-build", "--no-restore", "--nologo"],
            check=False,
        )
        if result.returncode:
            return result.returncode
    return 0


if __name__ == "__main__":
    sys.exit(main())
