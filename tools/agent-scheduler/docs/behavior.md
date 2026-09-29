# Scheduling and execution

Shared work overlaps freely. An exclusive ticket waits for admitted shared work
to drain and blocks later shared requests until it finishes (FIFO admission).
Use shared for ordinary work and exclusive for benchmarks, after build/setup.

`agent-scheduler status` shows this session's ticket. `agent-scheduler clear`
cancels a pending ticket. Interrupting a waiting Codex tool call also cancels it.
Immediately requesting the next ticket is supported: it waits asynchronously for
the previous release acknowledgement. No sleep or status polling is necessary.

## Scheduling rules

The launcher sets `AGENT_SCHEDULER_RULES` to the installed file:

```text
%LOCALAPPDATA%\NukeTheBees\agent-codex\bin\scheduling.rules
```

Installation replaces this copy. Make lasting changes in the repository's
[scheduling.rules](../scheduling.rules).

Scheduling rules are independent of Codex's security rules. All parsed command
components must be exempt; unrecognised/dynamic PowerShell syntax requires a ticket.
Exempt commands do not consume tickets and may run during exclusive work.
Other commands fail with instructions if no ticket exists. No ticket is inferred,
no command text is rewritten, and exemptions never bypass Codex security.

## Command lifetime

A ticket belongs to one logical command, including legitimate internal sandbox
retries. `UnifiedExecRuntime` calls the scheduler immediately before spawning;
the process manager accepts the final attempt after Codex's retry loop. An early
root exit retains the ticket until that retry/final decision is known.

The accepted attempt releases on **root-process exit**, independently of descendants
and output EOF. Ordinary pipe/PTY backends observe `child.wait()`; the restricted
backend signals after its Win32 root wait and exit-code query, before output
draining or ConPTY shutdown. The scheduler records the backend's root exit code;
Codex may separately report a logical timeout status such as `124`. Those diagnostic
codes can differ because timeout handling belongs to Codex's higher-level command
completion. Scheduler release does not wait for that final status or output EOF.
Codex's output draining and ConPTY teardown remain unchanged, so console descendants
may still be ended by Codex. The scheduler does not supervise or kill processes.

The jobserver must be available at startup; connection failure stops the session.
Daemon loss marks the session failed and wakes admission/release waits. Future
work fails, including exempt commands; restart Codex to reconnect. Already-running
processes remain Codex's responsibility.

## Supported scope

Windows local unified-exec supports ordinary and unelevated restricted-token
pipe/PTY backends, with one ticket/logical scheduled command per Codex process.
Elevated sandbox, MXC, remote and shell-snapshot execution are rejected. Elevated
runner support is deliberately omitted because its lifecycle regression requires
provisioned sandbox accounts/setup state. Its private IPC protocol is unchanged.
User `/shell`, hooks, MCP and app-server `command/exec` are outside this patch.
