#!/usr/bin/env python3
"""Cross-platform script to run clang-format on project source files."""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional


def check_clang_format() -> bool:
    """Check if clang-format is available in PATH."""
    if shutil.which("clang-format") is None:
        print("""ERROR: clang-format not found in PATH
Please install clang-format and ensure it's in your PATH
Installation instructions:
  - Windows: Install LLVM or Visual Studio Build Tools
  - macOS: brew install clang-format
  - Ubuntu/Debian: sudo apt install clang-format
  - Fedora/RHEL: sudo dnf install clang-tools-extra""", file=sys.stderr)
        return False
    return True


def get_file_extensions(format_hlsl: bool) -> set[str]:
    """Get the set of file extensions to format."""
    extensions = {".cpp", ".h", ".hpp", ".cc", ".cxx"}
    if format_hlsl:
        extensions.update({".hlsl", ".usf", ".ush"})
    return extensions


def find_files(directory: Path, extensions: set[str]) -> list[Path]:
    """Find all files with the given extensions recursively in the given directory."""
    files: list[Path] = []
    if not directory.exists():
        print(f"WARNING: Directory not found: {directory}")
        return files

    try:
        for file_path in directory.rglob("*"):
            if "generated" in file_path.parts:
                continue
            if file_path.is_file() and file_path.suffix.lower() in extensions:
                files.append(file_path.resolve())
    except PermissionError as error:
        print(f"WARNING: Permission denied accessing {directory}: {error}")
    except OSError as error:
        print(f"ERROR: Failed to scan {directory}: {error}")
    return files


def is_within_directory(file_path: Path, directory: Path) -> bool:
    """Return whether a file belongs to a formatting directory."""
    try:
        file_path.relative_to(directory)
        return True
    except ValueError:
        return False


def run_git(repository_root: Path, arguments: list[str]) -> bytes:
    """Run Git in the repository root and return its standard output."""
    command = ["git", *arguments]
    try:
        result = subprocess.run(command, cwd=repository_root, capture_output=True, timeout=30)
    except FileNotFoundError as error:
        raise RuntimeError("git not found in PATH") from error
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(f"{' '.join(command)} timed out") from error

    if result.returncode != 0:
        message = result.stderr.decode(errors="replace").strip()
        raise RuntimeError(message or f"{' '.join(command)} failed")
    return result.stdout


def get_repository_root(start_directory: Path) -> Path:
    """Find the Git repository root for a script invocation."""
    output = run_git(start_directory, ["rev-parse", "--show-toplevel"])
    return Path(output.decode(errors="surrogateescape").strip()).resolve()


def get_git_paths(repository_root: Path, arguments: list[str]) -> set[Path]:
    """Return repository-relative paths emitted by a null-delimited Git command."""
    output = run_git(repository_root, arguments)
    return {
        (repository_root / raw_path.decode(errors="surrogateescape")).resolve()
        for raw_path in output.split(b"\0")
        if raw_path
    }


def is_format_candidate(file_path: Path, directories: list[Path], extensions: set[str]) -> bool:
    """Return whether a path is an existing in-scope file supported by clang-format."""
    if "generated" in file_path.parts:
        return False
    if not file_path.is_file() or file_path.suffix.lower() not in extensions:
        return False
    return any(is_within_directory(file_path, directory) for directory in directories)


def select_staged_files(repository_root: Path, directories: list[Path], extensions: set[str]) -> list[Path]:
    """Select existing supported files staged for commit."""
    paths = get_git_paths(repository_root, ["diff", "--cached", "--name-only", "-z", "--diff-filter=ACMR"])
    return sorted(path for path in paths if is_format_candidate(path, directories, extensions))


def select_changed_files(repository_root: Path, directories: list[Path], extensions: set[str]) -> list[Path]:
    """Select changed and untracked supported files reported by Git."""
    paths = get_git_paths(repository_root, ["diff", "--name-only", "-z", "HEAD"])
    paths.update(get_git_paths(repository_root, ["ls-files", "--others", "--exclude-standard", "-z"]))
    return sorted(path for path in paths if is_format_candidate(path, directories, extensions))


def select_all_files(directories: list[Path], extensions: set[str]) -> list[Path]:
    """Select all supported files in the project formatting directories."""
    return sorted({file_path for directory in directories for file_path in find_files(directory, extensions)})


def select_unstaged_files(repository_root: Path, selected_files: list[Path]) -> list[Path]:
    """Return selected files that also have unstaged working-tree modifications."""
    unstaged_paths = get_git_paths(repository_root, ["diff", "--name-only", "-z"])
    return sorted(set(selected_files).intersection(unstaged_paths))


def stage_files(repository_root: Path, files: list[Path]) -> None:
    """Stage formatted files using repository-relative paths."""
    if not files:
        return
    relative_paths = [str(file_path.relative_to(repository_root)) for file_path in files]
    run_git(repository_root, ["add", "--", *relative_paths])


def format_file(file_path: Path) -> tuple[bool, Optional[str]]:
    """Format a single file with clang-format."""
    try:
        result = subprocess.run(["clang-format", "-i", str(file_path)], capture_output=True, text=True, timeout=30)
        if result.returncode == 0:
            return True, None
        return False, result.stderr.strip() or "Unknown error"
    except subprocess.TimeoutExpired:
        return False, "Timeout - file too large or clang-format hung"
    except OSError as error:
        return False, str(error)


def create_parser() -> argparse.ArgumentParser:
    """Create the command-line parser."""
    parser = argparse.ArgumentParser(description="Format C++ files with clang-format")
    parser.add_argument("--verbose", action="store_true", help="Show detailed progress for each file")
    parser.add_argument("--format-hlsl", action="store_true", default=True, help="Include HLSL files (default: True)")
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--staged", action="store_true", help="Format files staged for commit and restage them")
    modes.add_argument("--changed", action="store_true", help="Format files changed from HEAD and untracked non-ignored files")
    modes.add_argument("--all", action="store_true", help="Format all project source files (the default)")
    return parser


def main() -> None:
    """Format the selected project files."""
    args = create_parser().parse_args()
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent
    directories = [
        project_root / "Source",
        project_root / "Plugins" / "USFLoader",
        project_root / "Plugins" / "SandboxCore",
        project_root / "Plugins" / "SandboxEditorTools",
        project_root / "Plugins" / "SandboxMaterialExprs",
        project_root / "Plugins" / "SpaceGame",
    ]
    directories = [directory.resolve() for directory in directories]
    extensions = get_file_extensions(args.format_hlsl)
    repository_root: Optional[Path] = None

    try:
        if args.staged or args.changed:
            repository_root = get_repository_root(script_dir)
            if args.staged:
                files = select_staged_files(repository_root, directories, extensions)
                mixed_files = select_unstaged_files(repository_root, files)
                if mixed_files:
                    print("ERROR: Staged files also have unstaged edits:", file=sys.stderr)
                    for file_path in mixed_files:
                        print(f"  {file_path.relative_to(repository_root)}", file=sys.stderr)
                    print("Stage or stash the unstaged edits before running --staged.", file=sys.stderr)
                    sys.exit(1)
                description = "staged files"
            else:
                files = select_changed_files(repository_root, directories, extensions)
                description = "Git-changed files"
        else:
            files = select_all_files(directories, extensions)
            description = "all project source files"
    except RuntimeError as error:
        print(f"ERROR: Failed to select files: {error}", file=sys.stderr)
        sys.exit(1)

    print(f"Running clang-format on {description}.")
    if not check_clang_format():
        sys.exit(1)

    errors: list[str] = []
    for file_path in files:
        relative_path = file_path.relative_to(project_root)
        if args.verbose:
            print(f"Formatting: {relative_path}")
        success, error_message = format_file(file_path)
        if not success:
            errors.append(f"{relative_path}: {error_message}")
            print(f"ERROR formatting {relative_path}: {error_message}")

    if errors:
        print("\nErrors encountered:")
        for error in errors:
            print(f"  {error}")
        sys.exit(1)

    if args.staged:
        if repository_root is None:
            raise RuntimeError("Repository root was not resolved for staged formatting")
        try:
            stage_files(repository_root, files)
        except RuntimeError as error:
            print(f"ERROR: Failed to stage formatted files: {error}", file=sys.stderr)
            sys.exit(1)

    print(f"Successfully formatted {len(files)}/{len(files)} files.")


if __name__ == "__main__":
    main()
