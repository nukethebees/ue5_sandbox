#!/usr/bin/env python3
"""
Cross-platform script to run clang-format on all C++ files
in Source and Plugins/USFLoader directories recursively.
"""

import os
import sys
import subprocess
import shutil
from pathlib import Path


def check_clang_format():
    """Check if clang-format is available in PATH."""
    if shutil.which("clang-format") is None:
        print("ERROR: clang-format not found in PATH", file=sys.stderr)
        print("Please install clang-format and ensure it's in your PATH", file=sys.stderr)
        print("Installation instructions:", file=sys.stderr)
        print("  - Windows: Install LLVM or Visual Studio Build Tools", file=sys.stderr)
        print("  - macOS: brew install clang-format", file=sys.stderr)
        print("  - Ubuntu/Debian: sudo apt install clang-format", file=sys.stderr)
        print("  - Fedora/RHEL: sudo dnf install clang-tools-extra", file=sys.stderr)
        return False
    return True


def find_cpp_files(directory):
    """Find all C++ files recursively in the given directory."""
    cpp_extensions = {'.cpp', '.h', '.hpp', '.cc', '.cxx'}
    cpp_files = []

    if not directory.exists():
        print(f"WARNING: Directory not found: {directory}")
        return cpp_files

    try:
        for file_path in directory.rglob('*'):
            if file_path.is_file() and file_path.suffix.lower() in cpp_extensions:
                cpp_files.append(file_path)
    except PermissionError as e:
        print(f"WARNING: Permission denied accessing {directory}: {e}")
    except Exception as e:
        print(f"ERROR: Failed to scan {directory}: {e}")

    return cpp_files


def format_file(file_path):
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


def main():
    """Main function to format all C++ files."""
    print("Starting C++ file formatting...")

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

    total_files = 0
    formatted_files = 0
    errors = []

    # Process each directory
    for directory in directories:
        print(f"Processing directory: {directory.relative_to(script_dir)}")

        cpp_files = find_cpp_files(directory)

        for file_path in cpp_files:
            total_files += 1
            relative_path = file_path.relative_to(script_dir)
            print(f"Formatting: {relative_path}")

            success, error_msg = format_file(file_path)

            if success:
                formatted_files += 1
            else:
                errors.append(f"{relative_path}: {error_msg}")
                print(f"ERROR formatting {relative_path}: {error_msg}")

    # Print summary
    print(f"\nFormatting complete!")
    print(f"Total files processed: {total_files}")
    print(f"Successfully formatted: {formatted_files}")

    if errors:
        print(f"Files with errors: {len(errors)}")
        print("\nErrors encountered:")
        for error in errors:
            print(f"  {error}")
        sys.exit(1)
    else:
        print("All files formatted successfully!")


if __name__ == "__main__":
    main()