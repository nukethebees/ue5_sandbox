# Rust tools

Run Cargo commands from this directory to use the pinned Rust toolchain.

| Crate | Purpose |
| --- | --- |
| [agent-task](crates/agent-task/README.md) | Worktree preparation, feature Git operations, and developer workflows |
| `native-binary-tools` | Mimalloc symbol generation and auditing |
| `game-package-tools` | Packaged-game verification |
| `benchmark-tools` | [Benchmark orchestration and reports](../../docs/benchmarks.md) |

CMake builds the three domain tools privately on demand; they are not installed on PATH.
For AgentTask installation and commands, see its [local guide](crates/agent-task/README.md).

Run the complete tool suite with `cmake --workflow --preset tool-tests` from the repository
root. All four crates have matching CTest labels, including `agent-task`; CTest supplies
the configured temporary directory on Windows. Direct Cargo runs use the caller's temporary
environment: `cargo test --locked -p <package>` or `cargo test --workspace --locked`.
All test code lives in each crate's `tests/` directory.
