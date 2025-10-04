#!/usr/bin/env python3
"""
Cross-platform script to run clang-format on all C++ files
in Source and Plugins/USFLoader directories recursively.
"""

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
            if file_path.is_file() and file_path.suffix.lower() in extensions:
                files.append(file_path)
    except PermissionError as e:
        print(f"WARNING: Permission denied accessing {directory}: {e}")
    except Exception as e:
        print(f"ERROR: Failed to scan {directory}: {e}")

    return files


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
    args = parser.parse_args()

    print("Running clang-format recursively on the current directory.")

    # Check if clang-format is available
    if not check_clang_format():
        sys.exit(1)

    # Get script directory to handle relative paths correctly
    script_dir = Path(__file__).parent

    # Define directories to search (relative to script location)
    directories = [
        script_dir / "Source",
        script_dir / "Plugins" / "USFLoader"
    ]

    # Get file extensions to format
    extensions = get_file_extensions(args.format_hlsl)

    total_files = 0
    formatted_files = 0
    errors = []

    # Process each directory
    for directory in directories:
        if args.verbose:
            print(f"Processing directory: {directory.relative_to(script_dir)}")

        cpp_files = find_files(directory, extensions)

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