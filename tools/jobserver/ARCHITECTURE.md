# Admission gate

Modified Codex maintains one persistent connection to the per-user Windows named pipe
`\\.\pipe\NukeTheBees.Jobserver.<user SID>`. Each connection owns at most one ticket:
none, queued shared/exclusive, or granted shared/exclusive.

Tickets enter one FIFO queue. The server grants the leading shared group concurrently.
An exclusive ticket is granted only at the front, and forms a barrier to everything behind it.
Release or disconnect removes the ticket and admits the next eligible group.
No ticket expiry, execution timeout, renewal, watchdog, upgrades, or nesting exists.

## Protocol 3

Frames contain a little-endian 32-bit byte length followed by UTF-8 JSON (maximum 1 MiB).
Both endpoints require `hello` with `protocol.major = 3`; the server returns `hello_ack`.

The scheduling endpoint accepts only:

- `request` with `mode` (shared/exclusive) and a short human `name`; replies `queued` or `granted`.
- An unsolicited `granted` when a queued request reaches admission.
- `release`, which clears either queued or granted admission and replies `released`.

A second request while a ticket exists is an error. Identity is the connection, so no persistent
ticket identifiers, command descriptions, process IDs, exit codes, or resume tokens are needed.

The `.control` endpoint accepts `status`, `trace`, `ping`, and an idle-only `shutdown`.
It cannot acquire or release tickets. `trace` returns the last 1,000 in-memory diagnostic events;
history is discarded on restart. The CLI's explicit `start` invokes the fixed registered
NukeTheBeesJobserver task. Neither endpoint accepts executable names or program arguments.

## Cooperative identity

Both endpoints check the peer's Windows token user against the daemon's user.
For admission, `GetNamedPipeClientProcessId` and `QueryFullProcessImageNameW` must identify
`%LOCALAPPDATA%\NukeTheBees\agent-codex\bin\codex-scheduler.exe`.
The server rejects a duplicate connection from that process.
Codex also verifies that its pipe peer is the canonical installed `jobserverd.exe`.

This enforces local architecture, not a hostile-user security boundary. There are no credentials,
certificates, child-process permissions, or process-tree inspections. In-process server tests
supply a private endpoint and expected test executable; the production daemon exposes no identity override.

## Lifetime and limitations

A ticket lasts until the logical Codex command call returns control, including a normal
early/yielding return. Sandbox retries remain inside that call and are invisible to admission.
Exempt commands run normally and retain any outstanding ticket. Approval/security policy is independent.

Connection loss clears the client's ticket immediately. There is no reconnection, crash recovery,
session resumption, process supervision, root-exit observation, or descendant tracking.
A crashed Codex or a returned command may leave processes running. Rare overlap with a later
benchmark is accepted; rerun the benchmark. No nested scheduling or Codex sub-agents are supported.
Ordinary tools know nothing about scheduling; all admission happens above their execution boundary.

## Validation

Build `jobserver-tests`, `jobserver-gate-tests`, `jobserver`, and `jobserverd`, then run
`ctest --test-dir out/build/native -L jobserver --output-on-failure`.
Tests cover FIFO sharing/barriers, deterministic grants, duplicate requests, disconnect removal,
Windows peer identity, duplicate connections, diagnostics, framing, and rejection of removed CLI commands.
