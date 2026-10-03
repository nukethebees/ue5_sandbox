# PowerShell scripts

Run these scripts directly from the repository root; no shell setup is required.

## Install coj

The maintainer tests and installs the Rust CLI with:

```powershell
pwsh -NoProfile -File PowerShell/InstallCoj.ps1
```

Set `IOJ_ROOT` before installing. `-InstallRoot` overrides the default `%IOJ_ROOT%\tools\coj` destination;
`-LinkDirectory` overrides the shared tool-link directory. The script checks symlink support,
runs package tests, installs with the pinned Rust toolchain, smoke-tests the executable, and
publishes its tool link. See [developer tools](../tools/README.md) for PATH setup.
The installer sets `TMP` and `TEMP` to `%IOJ_ROOT%\tmp\<worktree-name>` for Cargo and its tests,
creating it if needed.
Agents assume the CLI is installed and report missing tools to the maintainer.

Use `-SkipTests` to retry installation after tests have already passed, for example after
fixing a file-access error. Installation and the executable smoke check still run.

Use `coj prepare-worktree` to begin tasks and `coj git` for feature Git operations.
After validation and explicit user authorization, run `coj integrate`; use
`--keep-branch` to retain the feature branch. See [coj usage](../tools/rust/crates/coj/docs/usage.md).

## Packaging and checks

- `PackageGame.ps1` composes CMake and CTest commands for Development or Shipping packages.
- `tests/TestDeveloperScripts.ps1` checks the syntax of scripts in this directory.
- `tests/TestUnrealEditorConfigurationTransition.ps1` builds and smoke-tests the Editor in
  DebugGame, Development, then DebugGame again. Use an exclusive
  [jobs-board ticket](../tools/jobserver/README.md) for the whole sequence.

See [Build and test](../docs/build-and-test.md) for direct CMake build commands, packaging,
project-file generation, and Editor launches.
