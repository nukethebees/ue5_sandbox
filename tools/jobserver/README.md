# Jobs board

A small cooperative, per-user Windows FIFO board. Agents use stock Codex and run commands
themselves. The server only maintains tickets; it never executes or monitors work.

Cheap work needs no ticket. Use `shared` for heavyweight builds/tests and `exclusive` for
benchmarks or other work requiring a quiet machine.

```powershell
agent-task jobs request shared "build tools"
# Returns a ticket ID and Queued or Ready immediately. Substitute that ID below.
agent-task jobs check 41
# Check periodically until Ready, then immediately before launching the command:
agent-task jobs start 41
cmake --build --preset native --target benchmark-tools-host
# Immediately after the command returns, including failure:
agent-task jobs end 41
```

`request exclusive "benchmark"` follows the same protocol. Later shared tickets wait behind
an earlier exclusive ticket. Ready reserves your turn but does not mean work has started.

```powershell
agent-task jobs status
agent-task jobs status --json
agent-task jobs cancel 42
```

Cancel only Queued/Ready tickets. Running tickets require `end`. Any local same-user caller can
manage a ticket by ID; there is no ownership authentication.

Forgotten tickets can block the board indefinitely; this remains an accepted trade-off. If another
agent's ticket appears stuck, report its ID and concern to the maintainer once, then wait for instructions.
Do not investigate or resolve it yourself; cleanup requires explicit maintainer direction.
There is no expiry, process tracking, automatic completion,
or crash recovery. Command completion means control returned to the agent, which must call `end`;
lingering descendants and rare measurement overlap are accepted limitations.

The maintainer installs AgentTask and runs `agent-task install-central-tools` to install the board
and its `NukeTheBeesJobserver` logon task. Report missing tools to the maintainer; agents do not install
them. Administration is limited to `jobserver --version`, `ping`, and `shutdown` (empty board only).
Start an installed stopped board with `Start-ScheduledTask -TaskName NukeTheBeesJobserver`.
Tickets exist only in memory; daemon restart starts an empty board with IDs beginning at 1.

See [architecture and focused tests](ARCHITECTURE.md).
