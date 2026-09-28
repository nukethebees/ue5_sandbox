# Codex scheduler example (Windows)

A small downstream patch connects Codex to the C++ jobserver. Codex keeps its
normal approval, sandbox and process-execution decisions. The external Rust
scheduler requires explicit tickets for commands not exempted by its own rules.

## Build and launch

The preparation script checks out the revision in `upstream-revision.txt` under
`.local/codex-upstream` and applies `codex.patch`. Build with Cargo/CMake; nothing
is installed over your normal Codex. The installed jobserver must be available
when starting a session; connection failure stops startup.

```powershell
tools/agent-scheduler/Prepare-Upstream.ps1
cmake -S tools/agent-scheduler -B .local/scheduler-build -G Ninja
cmake --build .local/scheduler-build --target scheduler-example codex-scheduler
cmake --build .local/scheduler-build --target scheduler-unit-tests codex-process-tests
tools/agent-scheduler/agent-codex.ps1 -c 'windows.sandbox="unelevated"' -c features.shell_snapshot=false
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
Shared work overlaps freely. An exclusive ticket waits for admitted shared work
to drain and blocks later shared requests until it finishes (FIFO admission).
Use shared for ordinary work and exclusive for benchmarks, after build/setup.

`scheduling.rules` is independent of Codex's security rules. All parsed components
must be exempt; unrecognised/dynamic PowerShell syntax requires a ticket.
Exempt commands do not consume tickets and may run during exclusive work.
Other commands fail with instructions if no ticket exists. No ticket is inferred,
no command text is rewritten, and exemptions never bypass Codex security.

## Command lifetime and scope

A ticket belongs to one logical command, including legitimate internal sandbox
retries. `UnifiedExecRuntime` calls the scheduler immediately before spawning;
the process manager accepts the final attempt after Codex's retry loop. An early
root exit retains the ticket until that retry/final decision is known.

The accepted attempt releases on **root-process exit**, independently of descendants
and output EOF. Ordinary pipe/PTY backends observe `child.wait()`; the restricted
backend signals after its Win32 root wait and exit-code query, before output
draining or ConPTY shutdown. Root notification uses the backend's completion code.
Codex's output draining and ConPTY teardown remain unchanged, so console descendants
may still be ended by Codex. The scheduler does not supervise or kill processes.

Immediately requesting the next ticket is supported: it waits asynchronously for
the previous release acknowledgement. No sleep or status polling is necessary.
Daemon loss marks the session failed and wakes admission/release waits. Future
work fails, including exempt commands; restart Codex to reconnect. Already-running
processes remain Codex's responsibility.

Supported: Windows local unified-exec, ordinary and unelevated restricted-token
pipe/PTY backends, one ticket/logical scheduled command per Codex process.
Elevated sandbox, MXC, remote and shell-snapshot execution are rejected. Elevated
runner support is deliberately omitted because its lifecycle regression requires
provisioned sandbox accounts/setup state. Its private IPC protocol is unchanged.
User `/shell`, hooks, MCP and app-server `command/exec` are outside this patch.
There is no central-tools installation integration.

## Validation

Run `tools/agent-scheduler/Run-Examples.ps1` from the repository root. It uses an
isolated installed daemon and scripted local Responses events; no model service
is contacted. Coverage includes FIFO admission, cancellation/grant races, spawn
failure, immediate ticket reuse, daemon loss, independent security rejection,
startup failure and a real sandbox-denied write followed by an approved retry.
`-Only lifecycle` or `-Only leases` selects a focused group.

The real Codex descendant regression obtains exclusivity while the root is gone
and its child still holds inherited output handles (ordinary pipes/PTY and
restricted pipes). Restricted PTY also checks root release, allowing Codex's
existing ConPTY teardown to end the child. Unit tests hold release acknowledgements
to verify asynchronous waiting, cancellation and disconnect handling; process tests
check output draining.
