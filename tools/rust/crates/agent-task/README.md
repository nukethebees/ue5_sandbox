# AgentTask

The maintainer-installed CLI for worktree preparation, constrained feature Git operations,
formatting, Unreal build invocation, and jobs-board coordination.

Start a task from anywhere inside its worktree:

```powershell
agent-task prepare-worktree
```

This clears `out`, updates submodules, regenerates presets and code, and disables Live Coding
in existing saved settings. Build and test the affected targets afterward.

- [Installation and command guide](docs/usage.md)
- [Architecture and Git guardrails](ARCHITECTURE.md)
- [Repository workflow policy](../../../../AGENTS.md)

For local development, run these from `tools/rust`:

```powershell
cargo build --release --package agent-task --locked
cargo test --package agent-task --locked
```

Tests use temporary Git repositories and small fixtures; they do not build the game or clear
this repository's output.
