"""Run Cargo with the pinned Codex package builder's verified V8 artifacts."""

import os
import subprocess
import sys
from pathlib import Path

repo = Path(__file__).resolve().parents[2]
upstream = repo / ".local/codex-upstream"
os.environ["CODEX_REPO_ROOT"] = str(upstream)
sys.path.insert(0, str(upstream / "scripts"))

from codex_package.targets import TARGET_SPECS
from codex_package.v8 import resolve_codex_v8_cargo_env

print("Preparing pinned Codex V8 artifacts", flush=True)
environment = os.environ.copy()
environment.update(
    resolve_codex_v8_cargo_env(
        TARGET_SPECS["x86_64-pc-windows-msvc"], cache_root=repo / ".local/codex-v8"
    )
)
sys.exit(subprocess.run(sys.argv[1:], cwd=upstream / "codex-rs", env=environment).returncode)
