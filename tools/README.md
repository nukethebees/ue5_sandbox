# Developer tools

Set the `IOJ_ROOT` environment variable to an absolute directory for shared tools and
temporary files (for example, `C:\dev\ioj`). Set it before starting the agent or developer shell.
The maintainer installs stable tools explicitly under
`%IOJ_ROOT%\tools\<ToolName>\bin`, keeping each tool's complete runtime together.

| Central tool | Installation/update | Source version |
| --- | --- | --- |
| coj | `pwsh -NoProfile -File PowerShell/InstallCoj.ps1` from the repository root | Cargo.toml |
| jobserverd | `coj install-central-tools` | Protocol header |

Installers publish literal symlinks in `%IOJ_ROOT%\tools\bin`.
Add only that directory to PATH. Enable Windows Developer Mode before installing,
or grant your account the **Create symbolic links** right and sign out/in. Installers check
link creation before building or updating tools and refuse to overwrite unrelated files.
The two installers share only `install/ToolLinks.ps1`: destination selection, symlink preflight,
and collision-safe publication. Revision-local Rust tools use the workspace's pinned Rust toolchain.

`coj jobs` accesses the per-user jobs board. Its installer shuts down an empty board,
copies the binaries, registers the logon task, and verifies startup. See [jobserver](jobserver/README.md).
When migrating an existing installation, the installer also recognizes the daemon at the
registered task's old path. It refuses to shut down a board with outstanding tickets.
Agents assume central tools are installed, report missing commands, and never install/update them
or build local fallbacks. `--version` is for manual diagnosis; bump the source version when shipping changes.

Use stock Codex. This repository does not build or install it.

Windows CTest runs set `TMP` and `TEMP` to `%IOJ_ROOT%\tmp\<worktree-name>`, creating it if needed.
This applies to presets and direct `ctest --test-dir` calls. CTest reads `IOJ_ROOT` at run time;
the worktree name comes from the configured source directory. Agent sandboxes must allow writes
to the worktree's temp directory.
The coj installer and `coj codex start <name>` use the same directory, so
Cargo and other child tools inherit the location automatically. Tests clean up their own fixtures.
Rust tests also support direct Cargo runs: their shared test-support crate reads `IOJ_ROOT`
and derives the worktree from the current directory without relying on `TMP` or `TEMP`.

`coj prepare-worktree` clears output, updates submodules,
generates presets, disables Live Coding in existing
`Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`, and generates code.
Missing settings are left absent. Preparation never installs tools or performs a broad build.
See the [Rust guide](rust/README.md) for coj installation and feature Git operations.
After preparation, `coj install-central-tools` configures native and installs jobserver.

`coj format [--all|--changed|--staged] [--jobs N|-j N] [--verbose]` reads formatting roots,
extensions, and exclusions from the current checkout's `.code-format.json`. Styling stays in
`.clang-format`. Developer operations invoke `coj` from PATH. Use
`coj format --changed` for changed C++ files and `coj unreal` for
builds; `coj unreal-build` supplies the small build-script invocation boundary.

CMake builds revision-local tools privately in each configuration's output directory:

- `game-package-tools`: exact package and asset expectations.
- `native-binary-tools`: native object and mimalloc symbol-prefix integration.
- `benchmark-tools`: revision-specific presets, executable locations, scenarios, and baselines.
  `coj benchmark` builds `benchmark-tools-host` through CMake on demand.

All three are Rust crates built under `out/build/<configuration>/rust-tools/release/`.
They launch domain subprocesses directly. The caller manages jobs-board admission and cancellation;
the tools do not maintain process trees or scheduling state.

Other tools: [layout planner](layout_planner/README.md), [image lab](image_lab/README.md), and
[LLVM checks](../docs/clang-tidy.md). Run focused tests for affected tools.
