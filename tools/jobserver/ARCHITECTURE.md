# Jobs-board internals

`agent-task jobs` forwards six verbs to the installed per-user `jobserver.exe` CLI.
The CLI sends one JSON request over `\\.\pipe\NukeTheBees.Jobserver.<user SID>`, reads one response,
and closes. The daemon serves requests sequentially. Disconnecting has no effect on tickets.
There are no client sessions, ownership records, or limits on tickets per caller.

The pipe permits the current Windows user and rejects remote clients. Any ordinary same-user
process may connect. The transport retains a four-byte little-endian length prefix and a 1 MiB
payload limit. Each request carries `protocol: 4`; other versions are rejected.

| Request `type` | Fields | Result |
| --- | --- | --- |
| `request` | `mode`: shared/exclusive, `name` | New ticket, immediately Queued or Ready |
| `check` | `id` | Active ticket |
| `start` | `id` | Ready becomes Running |
| `end` | `id` | Running becomes Done and leaves board |
| `cancel` | `id` | Queued/Ready becomes Cancelled and leaves board |
| `status` | none | All active tickets in FIFO order |

Ticket responses contain `type: ticket`, numeric `id`, `mode`, `state`, and `name`.
Status contains `type: status` and a `tickets` array. The text CLI groups Running, Ready,
and Queued, preserving FIFO within each group. `--json` preserves global queue order.
Errors contain `type: error`, `code`, and `message`; CLI failures have a nonzero exit code.
`ping` and empty-board `shutdown` are the only daemon administration messages.

The board is one vector in request order. New entries are Queued. Recalculating readiness scans
from the front: leading shared entries become Ready; the first exclusive stops the scan and
becomes Ready only if it is the first entry. Ready and Running entries keep their place.
Removal after end/cancel recalculates readiness. Thus a ready exclusive reserves the machine,
an exclusive waits for all earlier shared tickets (including Ready ones), and later shared work
cannot jump its barrier. Invalid transitions leave the board unchanged.

```text
Queued -> Ready -> Running -> Done
Queued/Ready -> Cancelled
```

This is voluntary coordination. The server does not know whether any work actually runs.
It has no command execution, process tracking, automatic lifetime detection, leases, heartbeats,
expiry, persistence, recovery, command rules, or Codex integration. Ordinary tools know nothing
about the board. Callers explicitly start immediately before work and end when control returns.
Forgotten tickets block until someone manually fixes them. Crashed agents, lingering children,
and rare overlap are accepted; tickets are lost on daemon restart.

Build `jobserver`, `jobserverd`, `jobserver-board-tests`, and `jobserver-tests`, then run
`ctest --test-dir out/build/native -L jobserver --output-on-failure`.
Tests cover FIFO readiness, explicit transitions, cancellation, status, CLI parsing and real
named-pipe request/response round trips. AgentTask's focused Cargo tests cover its CLI facade.
