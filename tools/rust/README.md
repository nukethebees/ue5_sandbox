# Rust developer-tool experiments

This Cargo workspace holds small native developer-tool experiments. It does not establish Rust as
the replacement for every C#, Python, or C++ tool.
Rustup selects the pinned toolchain from `rust-toolchain.toml` when commands run in this directory.

## agent-task

Begin each new task from anywhere in its Git worktree with:

```powershell
agent-task start
```

This removes the worktree-root `out`, synchronizes and initializes recursive submodules,
regenerates CMake presets and code, then builds the `task-start` baseline. It stops on the first
failure and streams subprocess output. Git, Python, CMake, and the repository build prerequisites
must be available in the environment.

Install or update it with the pinned Rust toolchain and an existing native configuration:

```powershell
cmake --build --preset native --target install-agent-task
```

For a fresh checkout without generated presets, bootstrap with:

```powershell
Set-Location tools/rust
cargo install --path crates/agent-task --root "$env:LOCALAPPDATA/NukeTheBees" --locked --force
```

Add `%LOCALAPPDATA%\NukeTheBees\bin` to PATH. The installed executable lives outside `out`,
so it can clear build output while running. For focused build and test validation:

```powershell
cmake --build --preset native --target agent-task
ctest --test-dir out/build/native -L '^agent-task$' --output-on-failure
```

The CLI tests use temporary Git repositories and small Python/CMake fixtures; they do not build
the game or clear this repository's output.

## set-live-coding-disabled

`set-live-coding-disabled` is the first experiment. Build and test it with:

```powershell
Set-Location tools/rust
cargo build --release --package set-live-coding-disabled
cargo test --package set-live-coding-disabled
```

The canonical Windows installation is:

```text
%LOCALAPPDATA%\NukeTheBees\bin\set-live-coding-disabled.exe
```

From a configured repository build directory, install or update it explicitly with:

```powershell
cmake --build --preset <preset> --target install-set-live-coding-disabled
```

CMake call sites use that canonical path and do not fall back to repository build outputs. The
installer stages and smoke-tests a replacement before atomically activating it. It treats the
directory as shared per-user state.

A future jobserver installation barrier can admit this installer only after tool users have drained,
then stage, validate, and activate the replacement before allowing queued work to resume. This
experiment does not implement that barrier. The installed executable currently has an existence
check only; a future barrier must also account for worktrees expecting different tool versions.
