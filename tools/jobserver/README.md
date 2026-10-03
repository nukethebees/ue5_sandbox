# Jobs board

A small cooperative, per-user Windows FIFO board. Agents use stock Codex and run commands
themselves. The server only maintains tickets; it never executes or monitors work.

`coj jobs` is the command interface, backed by a Rust client crate linked into coj. `jobserverd` is the background server that owns the board. Tickets display their
Codex session name (or worktree directory name) and worktree path.

Cheap work needs no ticket. Use `shared` for heavyweight builds/tests and `exclusive` for
benchmarks or other work requiring a quiet machine.

Request a ticket with `coj jobs request shared "build tools"`, check it until Ready,
call `start` immediately before the command, and call `end` immediately when the command
returns, including failure. Cancel unused Queued/Ready tickets. Never jump a queued exclusive
ticket or manage another agent's ticket without explicit maintainer direction.

Use `coj jobs clear <id>` or `coj jobs clear --owner <name> [--worktree <path>]` for explicit
manual board cleanup. Clearing never affects processes. Agents need maintainer direction
before clearing another owner's tickets.

Run `coj jobs --help` for commands and `coj jobs <command> --help` for arguments.

- [Client guide](docs/client.md): ticket workflow, scheduling, output, and scripting.
- [Server guide](docs/server.md): installation, administration, access, and lifetime.
- [Architecture](ARCHITECTURE.md): source responsibilities, platform boundaries, and focused tests.
