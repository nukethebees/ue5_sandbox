# Jobs board

A small cooperative, per-user Windows FIFO board. Agents use stock Codex and run commands
themselves. The server only maintains tickets; it never executes or monitors work.

`jobserver` is the command-line client. `jobserverd` is the background server that owns the board.

Cheap work needs no ticket. Use `shared` for heavyweight builds/tests and `exclusive` for
benchmarks or other work requiring a quiet machine.

Request a ticket with `agent-task jobs request shared "build tools"`, check it until Ready,
call `start` immediately before the command, and call `end` immediately when the command
returns, including failure. Cancel unused Queued/Ready tickets. Never jump a queued exclusive
ticket or manage another agent's ticket without explicit maintainer direction.

Run `jobserver --help` for the command list, `jobserver <command> --help` for arguments and
options, or `jobserver --help-all` for all command help.

- [Client guide](docs/client.md): ticket workflow, scheduling, output, and scripting.
- [Server guide](docs/server.md): installation, administration, access, and lifetime.
- [Architecture](ARCHITECTURE.md): source responsibilities, platform boundaries, and focused tests.
