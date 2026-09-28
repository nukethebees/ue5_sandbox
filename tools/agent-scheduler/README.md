# Codex scheduler example (Windows)

A small downstream Codex patch keeps normal approvals and process execution in
Codex. A separate scheduling policy exempts cheap commands. Other commands need
an explicit ticket, wait for FIFO admission, and release on root exit.
The C++ daemon is unchanged.

## Build and launch

The pinned Codex checkout belongs at `.local/codex-upstream`; its revision is in
`upstream-revision.txt`. Apply `codex.patch` to that revision. The checkout needs
Git's `core.longpaths=true` on Windows. This example uses Cargo/CMake, not upstream
Bazel packaging, and does not install or replace your normal Codex.

```powershell
tools/agent-scheduler/Prepare-Upstream.ps1
cmake -S tools/agent-scheduler -B .local/scheduler-build -G Ninja
cmake --build .local/scheduler-build --target scheduler-example codex-scheduler
tools/agent-scheduler/agent-codex.ps1
```

Inside the custom Codex session, issue separate ordinary command-tool calls:

```powershell
agent-scheduler ticket shared "Build native"
cmake --build --preset native
agent-scheduler ticket exclusive "Measure benchmark"
# Run the benchmark command here.
```

`agent-scheduler status` shows this session's ticket. `agent-scheduler clear`
cancels a pending ticket. Interrupting a waiting Codex tool call also cancels it.
An exempt inspection command does not consume a ticket. A command without a
ticket fails with instructions; it never implicitly acquires one.

`scheduling.rules` is independent of Codex's security rules. All parsed components
must be exempt; unrecognised/dynamic PowerShell syntax requires a ticket. Command
text is never rewritten. Exempt inspections may run during exclusive work.

## Demonstration and scope

Run `tools/agent-scheduler/Run-Examples.ps1` from the repository root. It starts an
isolated instance of the installed daemon, then exercises exemptions, missing
tickets, FIFO waiting, cancellation, and exclusive admission while a completed
root's child lives on. It also drives the actual patched CLI with scripted local
Responses events to check normal approval rejection, ticket handling, root release,
and failure to start without the daemon. It makes no model-service requests.

This first example supports local Windows unified-exec commands, one ticket and
one scheduled command per Codex process. It is not integrated into central-tools
installation. User `/shell`, app-server `command/exec`, hooks, and MCP execution
are outside this patch. Remote and shell-snapshot execution are rejected.

The persistent pipe owns the daemon ClientId. Disconnect invalidates the session;
restart Codex to reconnect. The example retains Codex's existing child containment;
it does not add a new process supervisor or promise additional crash containment.
The daemon journal records the ticket name and command text (as a health payload);
root PID reporting is omitted because Codex's common process handle does not expose it.
