# Codex scheduler example (Windows)

A small downstream Codex patch keeps normal approvals and process execution in
Codex. A separate scheduling policy exempts cheap commands. Other commands need
an explicit ticket, wait for FIFO admission, and release on root exit. A ticket
belongs to one logical Codex command, including its legitimate internal sandbox
retry. Codex still makes every approval and sandbox decision.
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
cmake --build .local/scheduler-build --target scheduler-unit-tests codex-process-tests
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
After a command completes, immediately requesting the next ticket is supported.
The request asynchronously waits for any outstanding release acknowledgement;
there is no need to sleep or poll status.

`scheduling.rules` is independent of Codex's security rules. All parsed components
must be exempt; unrecognised/dynamic PowerShell syntax requires a ticket. Command
text is never rewritten. Exempt inspections may run during exclusive work.

## Demonstration and scope

Run `tools/agent-scheduler/Run-Examples.ps1` from the repository root. It starts an
isolated instance of the installed daemon. The demonstrations exercise concurrent
shared work, FIFO exclusive admission, cancellation/grant races, spawn failure,
immediate ticket reuse, daemon loss, and independent security rejection. Scripted
local Responses events drive actual patched Codex execution, including a real
sandbox-denied write and its approved retry using the original ticket. No model
service is contacted. `-Only lifecycle` or `-Only leases` runs a focused group.

The descendant regression starts a root through Codex's actual Windows backend.
Its child inherits stdout/stderr and waits for an explicit test release. Another
client obtains exclusivity while that child is alive and the root is gone. It
covers ordinary pipes/PTY and restricted-token pipes. Restricted PTY admission is
also checked, preserving Codex's existing ConPTY shutdown behaviour (which can
end console descendants). The scheduler never waits for descendants or output EOF.

Unit tests deliberately withhold a daemon release acknowledgement and check that
the next ticket waits, can be cancelled, and fails promptly on disconnection.

This first example supports local Windows unified-exec commands, one ticket and
one logical scheduled command per Codex process. It is not integrated into central-tools
installation. User `/shell`, app-server `command/exec`, hooks, and MCP execution
are outside this patch. Remote, shell-snapshot and MXC helper execution are
rejected. MXC's additional helper/SDK process lifetime is outside this prototype's
root-event contract.

The persistent pipe owns the daemon ClientId. Daemon loss marks the scheduler
failed and wakes queued admission and release-acknowledgement waits. Future work
fails clearly, including exempt commands; restart Codex to reconnect. An already
running process continues under Codex's normal ownership and cancellation. The
scheduler adds no process killing, supervisor, or crash-containment mechanism.
The daemon journal records the ticket name and command text (as a health payload);
root PID reporting is omitted because Codex's common process handle does not expose it.

## Patch boundaries

`UnifiedExecRuntime` owns an external `Invocation` for its approval/retry lifetime
and calls `before_spawn` just before the existing local spawn. The process manager
accepts the final attempt after the existing orchestrator returns successfully.
Rejected attempts keep the ticket for a legitimate retry; failure or cancellation
relinquishes it. No command arguments are rewritten.

A generic process observer distinguishes root exit from full output completion.
Ordinary pipe/PTY backends observe their existing `child.wait()` result. The legacy
sandbox signals after its Win32 root wait and exit-code query, before ConPTY or
output draining. The elevated runner sends a separate `RootExit` frame at the same
boundary; its existing final `Exit` frame still follows output draining. The local
build includes the matching command runner (private IPC version 7).
Elevated-runner changes are compiled; the demonstrations use the restricted-token
backend and do not provision machine-wide sandbox accounts or setup state.
Focused runner-protocol tests check that early root notification preserves later
output and final completion, and existing driver tests check output draining.

The observer processes root exit before Codex publishes normal completion. The
external crate owns all ticket generations, admission, retry state, cancellation,
and release acknowledgement. An unsuccessful early root remains reserved while
Codex decides whether to retry; the accepted final attempt releases on root exit.
Codex output draining otherwise remains unchanged.
