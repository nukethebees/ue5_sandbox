#!/usr/bin/env python3
"""Cross-platform script to run clang-format on project source files."""

import os
import sys
import subprocess
import shutil
import argparse
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
    extensions = {'.cpp', '.h', '.hpp', '.cc', '.cxx'}
    if format_hlsl:
        extensions.add('.hlsl')
        extensions.add('.usf')
        extensions.add('.ush')
    return extensions


def find_files(directory: Path, extensions: set[str]) -> list[Path]:
    """Find all files with the given extensions recursively in the given directory."""
    files = []

    if not directory.exists():
        print(f"WARNING: Directory not found: {directory}")
        return files

    try:
        for file_path in directory.rglob('*'):
            # Skip the generated directory
            if 'generated' in file_path.parts:
                continue
            if file_path.is_file() and file_path.suffix.lower() in extensions:
                files.append(file_path)
    except PermissionError as e:
        print(f"WARNING: Permission denied accessing {directory}: {e}")
    except Exception as e:
        print(f"ERROR: Failed to scan {directory}: {e}")

    return files


def is_within_directory(file_path: Path, directory: Path) -> bool:
    """Return whether a file belongs to a formatting directory."""
    try:
        file_path.relative_to(directory)
        return True
    except ValueError:
        return False


def find_changed_files(script_dir: Path, directories: list[Path], extensions: set[str]) -> list[Path]:
    """Find changed and untracked format candidates reported by Git."""
    changed_paths: set[Path] = set()

    for command in (
        ["git", "diff", "--name-only", "-z", "HEAD"],
        ["git", "ls-files", "--others", "--exclude-standard", "-z"],
    ):
        try:
            result = subprocess.run(
                command,
                cwd=script_dir,
                capture_output=True,
                timeout=30,
            )
        except FileNotFoundError:
            raise RuntimeError("git not found in PATH")
        except subprocess.TimeoutExpired:
            raise RuntimeError("git command timed out")

        if result.returncode != 0:
            error = result.stderr.decode(errors="replace").strip()
            raise RuntimeError(error or f"{' '.join(command)} failed")

        for raw_path in result.stdout.split(b'\0'):
            if raw_path:
                changed_paths.add((script_dir / raw_path.decode(errors="surrogateescape")).resolve())

    files = []
    for file_path in changed_paths:
        if 'generated' in file_path.parts:
            continue
        if not file_path.is_file() or file_path.suffix.lower() not in extensions:
            continue
        if any(is_within_directory(file_path, directory) for directory in directories):
            files.append(file_path)

    return sorted(files)


def format_file(file_path: Path) -> tuple[bool, Optional[str]]:
    """Format a single file with clang-format."""
    try:
        result = subprocess.run(
            ["clang-format", "-i", str(file_path)],
            capture_output=True,
            text=True,
            timeout=30  # Timeout after 30 seconds
        )

        if result.returncode == 0:
            return True, None
        else:
            return False, result.stderr.strip() or "Unknown error"

    except subprocess.TimeoutExpired:
        return False, "Timeout - file too large or clang-format hung"
    except Exception as e:
        return False, str(e)


def main() -> None:
    """Main function to format all C++ files."""
    parser = argparse.ArgumentParser(description="Format C++ files with clang-format")
    parser.add_argument("--verbose", action="store_true", help="Show detailed progress for each file")
    parser.add_argument("--format-hlsl", action="store_true", default=True, help="Include HLSL files (default: True)")
    parser.add_argument(
        "--changed",
        action="store_true",
        help="Format files changed from HEAD and untracked non-ignored files",
    )
    args = parser.parse_args()

    if args.changed:
        print("Running clang-format on Git-changed files.")
    else:
        print("Running clang-format recursively on project source directories.")

    # Check if clang-format is available
    if not check_clang_format():
        sys.exit(1)

    # Get script directory to handle relative paths correctly
    script_dir = Path(__file__).parent

    # Define directories to search (relative to script location)
    directories = [
        (script_dir / "Source").resolve(),
        (script_dir / "Plugins" / "USFLoader").resolve(),
        (script_dir / "Plugins" / "SandboxCore").resolve(),
        (script_dir / "Plugins" / "SandboxEditorTools").resolve(),
        (script_dir / "Plugins" / "SandboxMaterialExprs").resolve(),
    ]

    # Get file extensions to format
    extensions = get_file_extensions(args.format_hlsl)

    total_files = 0
    formatted_files = 0
    errors = []

    if args.changed:
        try:
            cpp_files = find_changed_files(script_dir, directories, extensions)
        except RuntimeError as error:
            print(f"ERROR: Failed to get changed files from Git: {error}", file=sys.stderr)
            sys.exit(1)
    else:
        cpp_files = []
        for directory in directories:
            if args.verbose:
                print(f"Processing directory: {directory.relative_to(script_dir)}")
            cpp_files.extend(find_files(directory, extensions))

    for file_path in cpp_files:
        total_files += 1
        relative_path = file_path.relative_to(script_dir)

        if args.verbose:
            print(f"Formatting: {relative_path}")

        success, error_msg = format_file(file_path)

        if success:
            formatted_files += 1
        else:
            errors.append(f"{relative_path}: {error_msg}")
            print(f"ERROR formatting {relative_path}: {error_msg}")

    # Print summary
    print(f"Successfully formatted {formatted_files}/{total_files} files.")

    if errors:
        print(f"\nErrors encountered:")
        for error in errors:
            print(f"  {error}")
        sys.exit(1)


if __name__ == "__main__":
    main()
