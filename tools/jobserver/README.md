# NukeTheBees Jobserver

The jobserver is a small, Windows-only scheduler and process supervisor for local development. It
replaces coordination through lock and ticket files with one per-user daemon that owns the live
queue and resource state for every worktree.

It is intentionally local. Clients communicate with the daemon through a per-user Windows named
pipe; there is no TCP listener, remote execution, or distributed scheduling.

## Components

- `jobserverd.exe` owns resource allocation, job state, process supervision, logs, and history.
- `jobserver.exe` is the command-line client used by CMake, PowerShell, Codex, and developers.
- `jobserver-client` is the typed C++ client library used by the CLI and available to native tools.
- Windows Job Objects contain supervised process trees and kill them if the daemon exits. Job
  membership is attached atomically during process creation, so a daemon crash cannot strand a
  child in the gap between creation and later supervision.

The production pipe name includes the current user's SID:

```text
\\.\pipe\NukeTheBees.Jobserver.<user-sid>
```

The pipe rejects remote clients and its access control list permits only the current user and
LocalSystem. Messages use a four-byte little-endian payload length followed by UTF-8 JSON. The
current protocol version is `1.0`.

## Installation and startup

Configure and install the canonical per-user binaries with:

```powershell
cmake --preset win-x64-clangcl-debug
cmake --build --preset win-x64-clangcl-debug --target install-jobserver
```

`csetup` performs this bootstrap automatically when the jobserver is not installed.

The install target places immutable binaries under:

```text
%LOCALAPPDATA%\NukeTheBees\jobserver\bin
```

Mutable history and logs live separately under:

```text
%LOCALAPPDATA%\NukeTheBees\jobserver\data
```

Installation registers the per-user `NukeTheBeesJobserver` Scheduled Task. It starts at logon and
clients also ask the task to start when the pipe is absent. Task configuration uses `IgnoreNew`,
while `FILE_FLAG_FIRST_PIPE_INSTANCE` prevents two daemon processes from becoming authorities.

An update first asks the old daemon to shut down. Shutdown is refused while jobs are queued or
running, so installation cannot silently abandon or kill active work. The new binaries are copied
only after the old daemon drains, and the task is then registered and started again.

## Resource model

A job requests all of its resources atomically. It never holds one resource while waiting for
another, which avoids resource-order deadlocks.

Claims have three modes:

- Counted: consumes a number of capacity units, such as `cpu=8`.
- Shared: may overlap other shared users, such as ordinary work on `machine`.
- Exclusive: waits for every user of that resource to drain and then runs alone.

Built-in resources are:

| Resource | Capacity | Typical use |
|---|---:|---|
| `cpu` | Logical processor count | Explicit CPU budgets |
| `machine` | 1 | Shared builds and exclusive benchmarks |
| `benchmark` | 1 | Identifying and serializing benchmarks |
| `gpu` | 1 | Exclusive GPU work |

Unknown named resources are created with capacity one. Unreal builds use a resource derived from
the canonical engine checkout, so worktrees sharing an engine serialize with each other without
blocking an unrelated engine checkout.

Older conflicting requests take precedence. Independent jobs may pass each other, but later
shared work cannot starve an older exclusive request. Requests larger than a resource's capacity
are rejected instead of waiting forever.

## Running commands

Ordinary machine work:

```powershell
jobserver run --name "native tests" --kind test --shared machine -- ctest --preset native-tests
```

An exclusive benchmark:

```powershell
jobserver run `
  --name "fighter stress" `
  --kind benchmark `
  --exclusive machine `
  --exclusive benchmark `
  -- native-benchmark.exe --scenario fighter-stress
```

A counted request with timeout and hang suspicion:

```powershell
jobserver run `
  --name "parallel build" `
  --resource cpu=16 `
  --timeout 20m `
  --suspect-after 5m `
  -- cmake --build out/build/debug-game
```

`--worktree <path>` records the originating worktree. The child inherits the CLI's working
directory, so compiler launchers and other wrappers preserve their caller's relative paths.
`--detach` lets a supervised command continue if its submitting client disconnects. Attached jobs
are cancelled when their client disappears.

Commands already running inside a supervised job inherit `NUKETHEBEES_JOBSERVER_JOB`. A nested
CLI invocation launches locally inside the existing Windows Job Object rather than submitting a
second job that could deadlock behind its parent. The outer submission must therefore claim the
complete resource set needed by nested work.

## Inspecting and controlling work

```powershell
jobserver status
jobserver status --json
jobserver show <job-id>
jobserver history
jobserver logs <job-id>
jobserver cancel <job-id>
jobserver kill <job-id>
jobserver doctor
jobserver version
```

`status` reports active and queued jobs, blockers, elapsed time, health, and resource usage.
`cancel` and `kill` both terminate a supervised Windows Job Object; they use distinct conventional
exit codes (`130` and `137`). A timeout is reported as `124`.

Hang suspicion is diagnostic only. When `--suspect-after` is present, a running process tree is
marked `SUSPECTED_HANG` after the interval contains no stdout, stderr, or aggregate process-tree CPU
activity. New activity clears the suspicion. The daemon does not automatically kill a job based on
this heuristic.

## Failure behavior

- A lease is owned by its pipe connection and is released when that connection disappears.
- An attached supervised job is terminated when its client disconnects.
- A detached supervised job continues and remains visible through status and history.
- Handshake, request, and daemon-to-client writes have bounded deadlines. An incomplete or
  non-reading client cannot prevent daemon shutdown or upgrade; detached jobs continue logging
  after their client stops consuming output.
- Closing or crashing the daemon kills every supervised process tree through
  `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`.
- A restarted daemon begins with no live ownership. Active resource state is never reconstructed
  from disk.
- Completed-job history is append-only JSONL, tolerates corrupt lines, and is bounded to the newest
  1,000 entries. History is not authoritative for live ownership.
- History and logs are best-effort. An unavailable or full data directory does not prevent command
  execution or retain resource ownership.
- A protocol-major mismatch is rejected during the handshake. Minor versions may add compatible
  fields.

## C++ API

Native tools link `jobserver::client`. Transport and JSON details remain private to the library.

```cpp
auto lease = jobserver::Client::acquire({
    .metadata = {.name = "native benchmark", .kind = "benchmark", .worktree = root},
    .resources = {
        {.name = "machine", .mode = jobserver::ClaimMode::exclusive},
        {.name = "benchmark", .mode = jobserver::ClaimMode::exclusive},
    },
});
```

`Client::run` submits a typed `SubmitRequest`, streams stdout and stderr through a callback, and
returns the child exit code.

## Building and testing

```powershell
cmake --preset debug-game
cmake --build --preset debug-game --target jobserver jobserverd jobserver-tests
ctest --test-dir out/build/debug-game -R jobserver-tests --output-on-failure
```

Tests use a unique named pipe and temporary state directory, never the installed daemon or its
history. The integration suite starts real daemon and client processes and covers contention,
lease-client crashes, queued cancellation, restart, single-instance enforcement, and concurrent
shutdown/admission. Small helper executables provide deterministic output, crashes, sleeps, and
child process trees.

The opt-in system tests mutate the canonical per-user installation and create a temporary detached
Git worktree. Run them only when no other local work is using the jobserver:

```powershell
cmake --preset debug-game -DSANDBOX_JOBSERVER_SYSTEM_TESTS=ON
cmake --build --preset debug-game --target jobserver-tests
ctest --test-dir out/build/debug-game -L jobserver-system --output-on-failure
```

They validate a real CMake compile from a worktree path containing spaces and Unicode, cross-
worktree fairness and exclusivity, refused active updates, idle upgrades, concurrent daemon start,
and recovery when the installed client is missing. These tests are serial and are excluded from
the normal test presets.

The implementation uses Win32 directly rather than Boost.Process. Named-pipe security, suspended
launch followed by Job Object assignment, process-tree accounting, and reliable tree termination
all require native Windows APIs; adding the wider Boost headers would not remove that platform
plumbing here.
