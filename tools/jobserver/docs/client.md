# Jobs-board client

`coj jobs` is the single command interface. The client is a Rust crate linked into
coj; only `jobserverd` runs separately.

## Ticket workflow

Cheap work needs no ticket. Request `shared` for heavyweight builds/tests, or `exclusive`
for benchmarks and work needing a quiet machine.

```powershell
coj jobs request shared "build tools"
# Substitute the returned ID. Check periodically until Ready.
coj jobs check 41
# Immediately before running work:
coj jobs start 41
cmake --build --preset native --target benchmark-tools-host
# Immediately after the command returns, including failure:
coj jobs end 41
```

Ready reserves a place; it does not mean work has started. Shared tickets may run together.
Exclusive tickets wait for earlier work and block later shared tickets.

| Command after `coj jobs` | Purpose |
| --- | --- |
| `request <shared\|exclusive> <description> [--owner NAME]` | Create a ticket with owner/worktree metadata. |
| `check <id>` | Read an active ticket. |
| `start <id>` | Mark a Ready ticket Running. |
| `end <id>` | Remove a Running ticket after its command returns. |
| `cancel <id>` | Remove an unused Queued or Ready ticket. |
| `clear <id>` | Manually remove a ticket in any state. |
| `clear --owner NAME [--worktree PATH]` | Manually remove matching tickets in any state. |
| `status` | List active tickets, including owners and worktrees. |
| `ping` | Check daemon connectivity. |
| `shutdown` | Stop the daemon only when the board is empty. |

Use `coj jobs <command> --help` for arguments. Help and `coj --version` do not contact
the daemon. Invalid syntax reports an error and makes no board changes.

## Ownership and manual cleanup

Tickets inherit the name supplied to `coj codex start <name>` in the same worktree.
Without a named session, the owner defaults to the worktree directory name (current directory
outside Git). `request --owner NAME` selects an explicit owner.

Status displays the owner and normalized full worktree path. Owner names are labels, not
credentials or process identifiers. The same name can exist in multiple worktrees.

```powershell
coj jobs status
coj jobs clear 85
coj jobs clear --owner dev5
coj jobs clear --owner dev5 --worktree .
```

Owner clearing spans all worktrees unless limited by `--worktree`. Clearing reports the
removed tickets and releases their places on the board. It never stops processes or checks
whether work is still running. Process cleanup remains with `coj codex clean <name>`.

Agents must not clear another owner's tickets without explicit maintainer direction.
If a ticket appears stuck, report its ID once and wait for instructions.

## Output and lifetime

Add `--json` before or after the jobs subcommand for machine-readable successful replies.
Errors go to stderr with a nonzero exit code. Text status orders Running, Ready, then Queued;
JSON status preserves global FIFO order. Ended, cancelled, and cleared tickets disappear.

Tickets have no expiry, process tracking, automatic completion, or crash recovery. Restarting
the daemon loses the board. See the [server guide](server.md) for installation and administration.
