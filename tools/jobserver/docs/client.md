# Jobserver client

`jobserver` coordinates heavyweight work through a per-user jobs board. It does not launch
commands. `agent-task jobs` forwards the six ticket commands to the installed client.

## Ticket workflow

Cheap work needs no ticket. Request `shared` for heavyweight builds/tests, or `exclusive`
for benchmarks and other work that needs a quiet machine.

```powershell
agent-task jobs request shared "build tools"
# Substitute the returned ticket ID below.
agent-task jobs check 41
# Check periodically until Ready. Immediately before running the command:
agent-task jobs start 41
cmake --build --preset native --target benchmark-tools-host
# Immediately after the command returns, including failure:
agent-task jobs end 41
```

Requests return immediately as Queued or Ready. Ready reserves your place; it does not mean
work has started. Shared tickets may run together. Exclusive tickets wait for earlier work
and block later shared tickets. Never jump a queued exclusive ticket.

| Command | Purpose |
| --- | --- |
| `request <shared\|exclusive> <description>` | Create a ticket describing the planned work. |
| `check <id>` | Read a ticket's current state. |
| `start <id>` | Mark a Ready ticket Running immediately before starting work. |
| `end <id>` | Complete a Running ticket after the command returns, including failure. |
| `cancel <id>` | Remove an unused Queued or Ready ticket. |
| `status` | List active tickets. |

Use `jobserver <command> --help` for arguments and options, or `jobserver --help-all` for
all command help. Help and `--version` work without a running daemon.

## Output and scripting

```powershell
jobserver status
jobserver status --json
jobserver --json check 41
agent-task jobs status --json
```

Successful responses go to stdout. `--json` selects JSON output and may appear before or
after a direct `jobserver` command; with `agent-task jobs`, put the command first as shown.
Errors use stderr and a nonzero exit code. Successful commands, help, and version output
return zero. `--json` does not change help, version, or error output.

Text status groups tickets as Running, Ready, then Queued, preserving FIFO order within
each group. JSON status preserves global FIFO order. Completed and cancelled tickets are
removed from the board.

## Ticket lifetime and responsibility

Tickets survive client disconnection and have no expiry, process tracking, automatic
completion, or crash recovery. Forgotten tickets can block the board indefinitely. Command
completion means control returned to the caller, which must call `end`; lingering descendants
and rare measurement overlap are accepted limitations.

Any local caller running as the same user can manage a ticket by ID; the board does not track
ticket ownership. If another agent's ticket appears stuck, report its ID and your concern to
the maintainer once, then wait for instructions. Do not investigate or clean it up without
explicit maintainer direction.

See the [server guide](server.md) for installation, administration, and restart behaviour.
