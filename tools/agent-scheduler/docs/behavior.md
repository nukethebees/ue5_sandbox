# Scheduling behavior

The scheduler is a cooperative local FIFO shared/exclusive gate. Shared builds/tests may overlap.
An older exclusive request waits for earlier shared work and blocks later shared arrivals.
Each Codex client owns one connection and at most one ticket; duplicate requests and nested clients fail.

Request a ticket as a separate `exec_command` action using
`agent-scheduler ticket shared "operation"` or `agent-scheduler ticket exclusive "benchmark"`.
These pseudo-commands execute in Codex on its existing connection. They never spawn a helper.
The next non-exempt logical command consumes the ticket. Missing tickets are errors;
command text never infers or automatically requests admission.

## Scheduling rules

Active rules live at
`%LOCALAPPDATA%\NukeTheBees\config\agent-scheduler\scheduling.rules`.
The maintainer creates this file from the installed `scheduling.default.rules` and reviews exemptions.
Installation never creates or overwrites active rules. The wrapper loads them on its first command;
a missing or invalid file fails that command and subsequent commands without retrying setup.

Rules use Codex execpolicy prefix syntax, for example:

```python
prefix_rule(pattern=["rg"], decision="allow")
prefix_rule(pattern=["git", ["status", "diff"]], decision="allow")
```

Every parsed plain command must match an allow rule. Unparseable scripts require a ticket.
Rules are entirely independent from security approval: exempt commands still follow normal Codex policy.
An exempt command neither requires nor consumes an outstanding ticket.

## Logical lifetime

The scheduling wrapper surrounds Codex's logical command handler. It waits for grant, awaits the
existing handler (including sandbox approval/retries), then releases before returning its result.
Cancellation of the logical call also releases admission. There is no physical-attempt state.

Completion means the logical call returns control, including a yielding/background-session response.
It does not mean root-process exit, output EOF, or descendant completion. Ordinary tools receive
no scheduling environment variables and never contact the server.

Connection loss permanently fails the client and removes its queued/granted ticket at the server.
There is no reconnect/resume, crash recovery, timeout, expiry, lease renewal, or process tracking.
Surviving processes can occasionally overlap later work; that is accepted and a benchmark can be rerun.
No nested scheduling, sub-agents, or overlapping agents within one worktree are supported.
