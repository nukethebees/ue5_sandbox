# Jobserver server

`jobserverd` runs the per-user jobs board. It keeps tickets in memory and serves local client
requests sequentially. It never executes or monitors the work described by a ticket.

## Installation and startup

The maintainer installs coj and runs `coj install-central-tools` to install
jobserver under `%IOJ_ROOT%\tools\jobserver\bin` and register the
`NukeTheBeesJobserver` logon task. Agents report missing tools to the maintainer rather than
installing or updating them.

For a protocol upgrade, shut down the empty board using its existing matching client before
replacing either component. Install coj, then update the daemon.

Start an installed, stopped board with:

```powershell
Start-ScheduledTask -TaskName NukeTheBeesJobserver
```

## Administration

| Command | Purpose |
| --- | --- |
| `coj --version` | Show the client version without contacting the daemon. |
| `coj jobs ping` | Check daemon connectivity; a successful text reply is `pong`. |
| `coj jobs status` | Inspect active tickets before maintenance. |
| `coj jobs shutdown` | Stop the daemon only when the board is empty. |

Shutdown refuses while any Queued, Ready, or Running ticket remains. Coordinate with callers
before maintenance; do not complete or cancel another agent's ticket without explicit
maintainer direction.

Tickets are not persisted. Restarting the daemon loses all tickets and starts an empty board
with IDs beginning at 1. There is no process tracking or recovery of interrupted work.

## Access and platform support

The Windows backend uses named pipes restricted to the current user and rejects remote
connections. Ticket IDs are not ownership credentials: any local same-user caller can manage
them.

Windows is the only implemented transport platform. Protocol, board,
and CLI code are separate from the Windows backend; see the [architecture](../ARCHITECTURE.md)
for source responsibilities and the platform boundary.

See the [client guide](client.md) for the ticket workflow and scripting conventions.
