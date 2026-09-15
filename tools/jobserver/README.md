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
- The daemon and its supervised console processes run without allocating visible console windows.
  Nested commands preserve the same behavior.
- Windows Job Objects contain supervised process trees and kill them if the daemon exits. Job
  membership is attached atomically during process creation, so a daemon crash cannot strand a
  child in the gap between creation and later supervision.

The production pipe name includes the current user's SID:

```text
\\.\pipe\NukeTheBees.Jobserver.<user-sid>
```

The pipe rejects remote clients. Its access control list lets restricted local tokens connect, and
the daemon then impersonates each client and rejects it unless its user SID matches the daemon's
user SID. Messages use a four-byte little-endian payload length followed by UTF-8 JSON. The current
protocol version is `1.1`.

## Installation and startup

Configure and install the canonical per-user binaries with:

```powershell
cmake --preset win-x64-clangcl-debug
cmake --build --preset win-x64-clangcl-debug --target install-jobserver
```

`csetup` performs this bootstrap automatically when the jobserver is not installed.

The install target places the active binaries under:

```text
%LOCALAPPDATA%\NukeTheBees\jobserver\bin
```

Mutable history and logs live separately under:

```text
%LOCALAPPDATA%\NukeTheBees\jobserver\data
```

`jobserverd.log` records daemon lifecycle and internal diagnostic messages. At startup, a log of at
least 1 MiB is moved to `jobserverd.previous.log`; logging remains best-effort and cannot prevent
the daemon from starting.

Completed jobs leave the live scheduler table after their final state is captured for history;
history retains the newest 1,000 entries independently of live resource ownership. This also
applies to cancelled queues, disconnected clients, failed launches, and unavailable persistence.

Each job's stdout/stderr file is capped at 4 MiB. Output beyond that cap is discarded from disk,
with a truncation marker replacing the end of the file; connected clients still receive full output.
The newest 100 completed job log pairs are retained, in addition to active jobs. Cleanup runs after
log closure and on authoritative daemon startup, including logs left by a crash. Retained legacy
oversized files are truncated to the same cap. Expired logs are permanently deleted; history may
therefore describe a job whose logs are no longer available. Cleanup ignores unrelated files and
symlinks and is best-effort: disk failures never cancel jobs or strand resources.

Installation registers the per-user `NukeTheBeesJobserver` Scheduled Task. It starts at logon and
clients also ask the task to start when the pipe is absent. Task configuration uses `IgnoreNew`,
while `FILE_FLAG_FIRST_PIPE_INSTANCE` prevents two daemon processes from becoming authorities. A
global per-user startup mutex makes racing clients recheck the pipe instead of each invoking Task
Scheduler independently.

An update stages and smoke-tests both new binaries before asking the old daemon to shut down.
Shutdown is refused while jobs are queued or running, so installation cannot silently abandon or
kill active work. After the old daemon drains, the installer swaps the complete `bin` directory,
registers the task, and verifies the replacement through `ping`. The prior binaries are retained in
`previous` and are automatically restored and restarted if anything after the swap fails. A named
per-user mutex serializes installers from separate worktrees and Windows sessions. A later install
also repairs an update interrupted while the active directory was being swapped and removes
abandoned staging directories.

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
jobserver ping
jobserver recover --check
jobserver recover --force
jobserver show <job-id>
jobserver history
jobserver logs <job-id>
jobserver cancel <job-id>
jobserver kill <job-id>
jobserver doctor
jobserver version
```

`status` reports daemon PID, uptime, audit freshness, handler/owner counts, active and queued jobs,
blockers, elapsed time, health, and resource usage.
`doctor` checks the canonical binaries and client location, Scheduled Task action, responsive
daemon PID and executable, protocol, authority record, writable data directory, diagnostic log,
and abandoned installer staging directories. Failures produce a nonzero exit code; warnings are
reported without making the command fail.
Queued clients receive periodic heartbeat frames while they wait. `ping` uses a bounded control
request to distinguish a responsive daemon from one whose process merely still exists. Other
control commands also fail with a clear timeout rather than waiting indefinitely.
`recover --check` performs the same identity and liveness checks without changing anything.
`recover --force` is an explicit last resort for an unresponsive daemon. It refuses a responsive
daemon and terminates a process only after a global per-user recovery mutex and a live comparison
of the recorded PID, process creation time, executable path, and user SID. It never searches or
kills by process name. Recovery then asks the canonical Scheduled Task to start a replacement
daemon.
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
- Initial handshakes and requests time out after five seconds, and the daemon admits at most 64
  simultaneous client handlers. Excess connections are rejected and counted in `status`, bounding
  thread and handle growth during broken-client floods.
- Closing or crashing the daemon kills every supervised process tree through
  `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`.
- A restarted daemon begins with no live ownership. Active resource state is never reconstructed
  from disk.
- The daemon publishes a separate `authority.json` identity record for explicit stale-daemon
  recovery. It is not authoritative for resources or leases, is removed only by the matching
  process instance, and is fully revalidated before use so stale PIDs and PID reuse cannot select
  another process.
- Completed-job history is append-only JSONL, tolerates corrupt lines, and is bounded to the newest
  1,000 entries. History is not authoritative for live ownership.
- History and logs are best-effort. An unavailable or full data directory does not prevent command
  execution or retain resource ownership.
- A daemon audit loop checks live owner registration and recomputes resource accounting from the
  authoritative job queue. It interrupts expired `STARTING` entries, recovers ownerless jobs, and
  publishes recovery details in `status` under `diagnostics` (shown as `RECOVERIES` in text output).
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
worktree fairness and exclusivity, refused active updates, rollback after a failed binary swap,
recovery from an interrupted swap, idle upgrades, concurrent daemon start, and recovery when the
installed client is missing. These tests are serial and are excluded from the normal test presets.

The integration suite also uses deterministic lifecycle barriers to terminate the daemon after
admission, allocation, process creation, process resume, output, descendant creation, completion,
and history publication. Each case verifies that resources and test-process counts return to their
baseline.

## Nested commands and resource claims

Commands launched inside an existing supervised job execute locally in its inherited Windows Job
Object instead of entering the queue again. This prevents a nested command from waiting behind
its own parent. Commands with no resource claims need no daemon round trip. Commands requesting
resources must first obtain daemon validation: the caller must belong to the claimed parent's
process tree, the parent must be running, and every requested claim must fit its allocation.

An exclusive parent claim covers any valid claim on that resource. Shared covers only shared;
counted covers only counted requests up to the parent's allocation. Insufficient claims fail with
`nested_resource_not_held`; stale parents fail with `nested_parent_not_active`; copied job IDs from
unrelated processes fail with `nested_parent_mismatch`. Nothing is queued or launched on rejection.
Active claims are visible in `status` and `show`.

Nested commands retain inherited output handles and their requested working directory, remain
hidden, and are killed with the parent's process tree on cancellation or daemon failure. Nested
timeouts, activity callbacks, and narrower hang policies are not independently applied: the
parent's supervision policy governs their lifetime.

`Command::EnvironmentChange` adds/replaces a variable when `value` contains a string and removes
it when `value` is `std::nullopt`. Changes apply in order, case-insensitively; the last change wins.
Both normal and nested launches apply these changes. The job ID variable is always daemon-owned
and cannot be replaced or removed by command overrides.

Protocol 1.1 adds nested validation and nullable environment values. New clients fail closed if an
older daemon does not support nested validation. Upgrade the installed CLI and daemon together;
rebuild native tools statically linked to the client library, since old binaries retain the previous
nested bypass behavior. The Tracy comparison driver uses a private child marker rather than
treating any enclosing job as permission to run benchmarks.

The implementation uses Win32 directly rather than Boost.Process. Named-pipe security, suspended
launch followed by Job Object assignment, process-tree accounting, and reliable tree termination
all require native Windows APIs; adding the wider Boost headers would not remove that platform
plumbing here.
