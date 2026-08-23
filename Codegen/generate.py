from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from Codegen.manifest import modules
from Codegen.manifests.common import PROJECT_ROOT
from Codegen.cpp import CppFile, Module


@dataclass(frozen=True)
class GeneratedFile:
    file: CppFile
    content: str


def collect_files(generated_modules: tuple[Module, ...]) -> tuple[CppFile, ...]:
    files = tuple(file for module in generated_modules for file in module.files())
    paths = [file.path.resolve() for file in files]
    duplicates = sorted({path for path in paths if paths.count(path) > 1})
    if duplicates:
        relative_paths = [str(path.relative_to(PROJECT_ROOT)) for path in duplicates]
        raise ValueError(f"Duplicate generated output paths: {', '.join(relative_paths)}")
    return files


def render_files(generated_modules: tuple[Module, ...]) -> tuple[GeneratedFile, ...]:
    return tuple(
        GeneratedFile(file=file, content=file.render())
        for file in collect_files(generated_modules)
    )


def is_current(generated_file: GeneratedFile) -> bool:
    path = generated_file.file.path
    return path.exists() and path.read_text(encoding="utf-8") == generated_file.content


def relative_path(path: Path) -> Path:
    return path.resolve().relative_to(PROJECT_ROOT)


def generate(check_only: bool) -> int:
    stale_files: list[Path] = []
    for generated_file in render_files(modules()):
        path = generated_file.file.path
        if is_current(generated_file):
            print(f"Unchanged {relative_path(path)}")
            continue
        if check_only:
            stale_files.append(path)
            print(f"Stale {relative_path(path)}")
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(generated_file.content, encoding="utf-8", newline="\n")
        print(f"Wrote {relative_path(path)}")

    if stale_files:
        print("Generated files are stale. Run: python3 -m Codegen.generate")
        return 1
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate committed Sandbox C++ files")
    parser.add_argument(
        "--check",
        action="store_true",
        help="Report stale generated files without writing them",
    )
    args = parser.parse_args()
    return generate(check_only=args.check)


if __name__ == "__main__":
    raise SystemExit(main())
