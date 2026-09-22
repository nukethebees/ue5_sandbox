# Rust developer-tool experiments

This Cargo workspace holds small native developer-tool experiments. It does not establish Rust as
the replacement for every C#, Python, or C++ tool.
Rustup selects the pinned toolchain from `rust-toolchain.toml` when commands run in this directory.

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
