"""Translate the project's Codex build profile to its CMake configuration."""

import sys
import tomllib
from pathlib import Path


def read_configuration(path: Path) -> str:
    with path.open("rb") as source:
        config = tomllib.load(source)
    profile = config["tools"]["codex"]["build_profile"]
    configurations = {
        "debug": "Debug",
        "release-no-lto": "ReleaseNoLTO",
        "release": "Release",
    }
    if not isinstance(profile, str) or profile not in configurations:
        raise ValueError("tools.codex.build_profile must be debug, release-no-lto, or release")
    return configurations[profile]


if __name__ == "__main__":
    path = Path(__file__).resolve().parents[2] / "ioj.toml"
    try:
        print(read_configuration(path))
    except (OSError, KeyError, TypeError, ValueError) as error:
        sys.exit(f"Cannot read tools.codex.build_profile from {path}: {error}")
