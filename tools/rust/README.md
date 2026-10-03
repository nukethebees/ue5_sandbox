# Rust tools

Run Cargo commands from this directory to use the pinned Rust toolchain.

| Crate | Purpose |
| --- | --- |
| [agent-task](crates/agent-task/README.md) | Worktree preparation, feature Git operations, and developer workflows |
| `native-binary-tools` | Mimalloc symbol generation and auditing |
| `game-package-tools` | Packaged-game verification |
| `benchmark-tools` | [Benchmark orchestration and reports](../../docs/benchmarks.md) |
| `ioj-test-support` | Shared worktree temporary directories for Rust tests |

CMake builds the three domain tools privately on demand; they are not installed on PATH.
For AgentTask installation and commands, see its [local guide](crates/agent-task/README.md).

Run the complete tool suite with `cmake --workflow --preset tool-tests` from the repository
root. All four tool crates have matching CTest labels, including `agent-task`.
For direct runs, use `cargo test --locked -p <package>` or `cargo test --workspace --locked`.
Set `IOJ_ROOT` first. Tests use `ioj-test-support` to create `%IOJ_ROOT%\tmp\<worktree-name>`,
deriving the checkout from the current directory. `temp_root()` returns that deterministic base;
`temp_dir(prefix)` creates an isolated fixture there and removes it on drop.
Rust test fixtures do not depend on `TMP`/`TEMP` or a CMake launcher.
All test code lives in each crate's `tests/` directory.
