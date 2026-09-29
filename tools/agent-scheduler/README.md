# Codex scheduler (Windows)

Explicit jobserver tickets for Codex commands. Shared work overlaps; exclusive
benchmarks wait their turn and run alone. Codex keeps its normal security checks.

## Install and launch

From the repository root:

```powershell
. ./dev.ps1
install-agent-task
agent-task install-central-tools
```

Add `%LOCALAPPDATA%\NukeTheBees\agent-codex\bin` to PATH, then run
`agent-codex.ps1`. Normal `codex` remains unchanged.

## Use

Request a ticket in one command, then run the work in the next:

```powershell
agent-scheduler ticket shared "Build native"
cmake --build --preset native
```

Use `exclusive` for benchmarks after build/setup. `agent-scheduler status` shows
the ticket; `clear` cancels pending work. Cheap commands in `scheduling.rules`
need no ticket. Tickets release on root exit, even if descendants remain alive.

- [Scheduling, rules and supported execution paths](docs/behavior.md)
- [Installation details, local builds and validation](docs/development.md)
