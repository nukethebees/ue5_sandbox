# Jobserver architecture

`jobserverd.exe` owns client sessions, a FIFO lease queue, named shared/exclusive gates, and a
diagnostic journal. `jobserver.exe broker`, `jobserver.exe run`, and the C++ `Session` API use the
same admission protocol. The client executes commands locally. The daemon never parses command
text, launches processes, collects output, or tracks descendants.

## Admission and lifetime

Every request declares one to eight distinct named gates and their shared/exclusive modes. Claims
are granted atomically. A request waits behind every older conflicting request, whether that
request is queued or granted. Shared claims do not conflict with each other. Independent named
gates can proceed concurrently. There are no counted resources, CPU budgets, or command classifiers.

For the `machine` gate, shared A/B/C can all run. Exclusive D waits for all three. Later shared
E/F wait behind D. When A/B/C release, D runs alone; after D releases, E/F can both run. Multiple
exclusive requests retain their FIFO order. Engine gates and `integration/dev` use this same rule.

A pipe handshake assigns a new `ClientId`. One session has at most one pending or granted command.
Each request receives a `CommandId` and `LeaseId`; metadata contains opaque strings such as command
text, cwd, worktree, task, and operation name. The connection is the liveness authority. Disconnect
cancels pending requests, releases grants, and wakes eligible waiters. Reconnect creates a new
session. Restart replays diagnostic events but never reconstructs leases or process ownership.
`GateId` values identify names within a daemon lifetime; event payloads retain the gate names.

The executor waits only for the locally launched root process. A successful `dotnet build` releases
its lease even if a compiler server remains alive. The persistent broker owns one Windows Job
Object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`, assigns each root at creation, and never waits
for the Job Object to empty. Descendants can survive individual commands, but broker exit/crash
kills the remaining session tree. A one-shot `run` ends its session after its root completes.
Daemon loss also ends the broker and its local tree. No Roslyn special cases are needed.

## Persistent broker interface

Start `jobserver broker` with stdin/stdout pipes. Each frame is a four-byte little-endian unsigned
UTF-8 byte length followed by one JSON object (maximum 1 MiB). stdout carries only framed control
messages. The initial response is `{"type":"ready","client":123,"protocol":1}`; this broker
interface version is independent of the daemon protocol major version.

Send a command with explicit gates:

```json
{"type":"command","text":"cmake --build --preset native","cwd":"C:/work/repo","gates":[{"name":"machine","mode":"shared"}],"metadata":{"name":"Native build","task":"feature"}}
```

The broker passes `text` unchanged to `pwsh -NoLogo -NoProfile -NonInteractive -Command`. It inherits
the broker's local environment; no environment snapshot crosses the daemon connection. Load the
repository environment before starting the broker, or explicitly dot-source `dev.ps1` in commands
that need its functions. Metadata, including command text and cwd, is limited to 8 KiB in total.

Responses include `queued`, `granted`, `starting`, `completed` (with `exit_code`), `cancelled`,
`error`, and `idle`. Admission responses identify client, command, and lease. `starting` additionally
names local stdout/stderr files under `<cwd>/.local/jobserver-output/`. The broker writes command
output directly to these files; the daemon never ingests it. Command stdin is closed/NUL.

`{"type":"clear"}` cancels the current pending command. A successful clear yields `cancelled`
then `idle`, and never launches that command. Once execution begins, clear returns `not_pending`.
Wait for `idle` before submitting another command. A second command while busy returns `busy`.
`{"type":"quit"}` or input EOF ends the session and its remaining local processes.

## Transport and health

The retained user-SID-scoped named pipes use bounded JSON frames, protocol negotiation, blocking
overlapped Win32 reads, and a separate control endpoint. Protocol major 2 has `acquire`, `cancel`,
`started`, and `release` for leases, plus handshake, heartbeat, health, inspection, and lifecycle
messages. There is no `submit`, daemon output stream, environment forwarding, or process-owner API.

The daemon sends a heartbeat every five seconds while a connection has no other traffic. Clients
block on the pipe; a 20-second watchdog reports `server_unresponsive` and closes a silent session.
Heartbeats do not create journal rows. Health transitions and protocol errors do. A client makes
a bounded best-effort health report on connection failure; a dead daemon cannot record it, so the
broker also reports the diagnostic directly to its driver.

Shutdown refuses while any lease is queued or granted. Discovery, scheduled-task startup,
same-user authorization, doctor, and guarded recovery remain. Recovery never resurrects ownership.

## Build and benchmark boundaries

Admit ordinary agent commands once at their outer boundary through the broker or `run --shared
machine`. CMake, Ninja, compiler/linker invocations, and ordinary tests then run normally. Five
concurrent builds are fine. CMake has no jobserver compiler, linker, or test launcher.

BenchmarkTools, benchmark CMake targets, and tools/perf explicitly request `machine/exclusive`
for measurement, after build/setup. Do not wrap these orchestration commands in an outer shared
machine lease: that would make the benchmark wait for its own caller. Named engine transaction
gates can be acquired inside an already admitted operation. The local executor exposes explicit
lease/machine-mode markers for these existing integrations; they are not inferred from command text.
Unmanaged commands cannot be made quiet by a cooperative gate.

## Diagnostics and retention

`jobserver status` reports clients, active and queued commands, gate owners, the current exclusive
owner, and blocker lease IDs. `jobserver trace` returns recent events chronologically. Filters
`--client`, `--command`, `--lease`, `--gate`, and `--event` can be combined; `--limit` defaults to
100 and is capped at 1,000. A response is bounded to 768 KiB. Trace is a compact column scan.

The in-memory SoA journal retains up to 1,000,000 events: timestamps, event kinds, client/command/
lease/gate handles, related lease, numeric value, and payload handle occupy about 59 MiB. Strings
are interned separately with a 32 MiB text budget; container and intern-map overhead is additional.
The oldest events are evicted when either budget is reached.

Persistent logs are in `%LOCALAPPDATA%\NukeTheBees\jobserver\data`:

- `events.jsonl`: four 32 MiB segments (128 MiB total), including rotated files. Numeric event rows
  are `[timestamp_us, kind, client, command, lease, gate, related_lease, value, payload_id]`. Event
  kinds follow `EventKind` in `daemon/journal.hpp`; string records are `{"payload":id,"text":"..."}`.
  Definitions precede use within each segment. An interrupted trailing record is ignored on replay.
- `jobserverd.log`: four 16 MiB human-readable segments (64 MiB total), with timestamps and handles.

Transitions are flushed as they occur: daemon/client lifecycle, received commands, lease requests,
queue/blocker changes, grants, local starts, completions, releases, cancellations, exclusive
reservations, health, and protocol errors. There is no tight polling-loop logging. Command and
client handles can later associate output streams without changing scheduler lifetime semantics.
