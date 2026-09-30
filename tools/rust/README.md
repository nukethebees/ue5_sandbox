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

Run focused tests with `cargo test --locked -p <package>`, or all four crates with
`cargo test --workspace --locked`. Domain tools also have matching CTest labels;
AgentTask tests run directly through Cargo. All test code lives in each crate's `tests/` directory.
