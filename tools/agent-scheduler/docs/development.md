# Installation and development

## Central installation

Update `agent-task` once with `. ./dev.ps1` followed by `install-agent-task`, then
run `agent-task install-central-tools`. This prepares the pinned source, builds
Codex, its code-mode host, and the scheduler client using the root [ioj.toml](../../../ioj.toml), and installs them under
`%LOCALAPPDATA%\NukeTheBees\agent-codex\bin`. Close custom Codex sessions before updating.
This maintainer-only installer builds directly, without requesting a jobserver lease.

The `agent-codex.ps1` launcher forwards normal Codex arguments, selects the
unelevated backend, disables shell snapshots, and loads your external scheduling
rules ([setup](behavior.md#scheduling-rules)). Normal `codex` remains unchanged.
`codex-code-mode-host.exe` is a required companion that executes code-mode tool
programs such as `functions.exec`; it must be built and installed alongside Codex.
The build reuses the pinned upstream package builder to fetch and verify OpenAI's
V8 archive/bindings into `.local/codex-v8` before running Cargo.

The [standalone installer](../../install/Install-AgentCodex.ps1) accepts
`-InstallRoot <private-root>` and `-Configuration Debug` for staging validation
without replacing installed tools.

Set `[tools.codex].build_profile` to `debug` (the default), `release-no-lto`, or
`release`. The installer reads this on every run; no AgentTask rebuild is needed.
An explicit `-Configuration Debug`, `ReleaseNoLTO`, or `Release` overrides the file.
Python 3.11 or newer reads TOML using its standard library.

`release-no-lto` uses [build-profile.toml](../build-profile.toml): Cargo's
`release-no-lto` profile inherits each workspace's release settings and disables
all LTO with `lto = "off"`. Codex's other release settings remain unchanged.
Artifacts live under `.local/scheduler-target/release-no-lto`. Use
`-Configuration Release` for upstream's original release profile with ThinLTO.
Local CMake builds select the derived profile with `-DCMAKE_BUILD_TYPE=ReleaseNoLTO`.

## Local build

Run these commands from the repository root. The preparation script checks out
the revision in [upstream-revision.txt](../upstream-revision.txt) under
`.local/codex-upstream` and applies [codex.patch](../codex.patch).
The local build does not replace installed tools.

```powershell
tools/agent-scheduler/Prepare-Upstream.ps1
cmake -S tools/agent-scheduler -B .local/scheduler-build -G Ninja
cmake --build .local/scheduler-build --target scheduler-example codex-scheduler
tools/agent-scheduler/agent-codex.ps1
```

## Validation

From the repository root:

```powershell
cmake --build .local/scheduler-build --target scheduler-unit-tests codex-process-tests
tools/agent-scheduler/Run-Examples.ps1
```

The demonstrations use an isolated instance of the installed daemon and scripted
local Responses events; no model service is contacted. Run the launcher outside
Codex's sandbox: the child Codex creates its own restricted-token sandbox.
Coverage includes sandboxed `ticket`/`status`/`clear` access, FIFO
admission, cancellation/grant races, spawn failure, immediate ticket reuse,
daemon loss, independent security rejection, startup failure and a real
sandbox-denied write followed by an approved retry.

`-Only lifecycle` or `-Only leases` selects a focused group.
Use `-InstalledBin <install-root>/bin` to exercise the installed launcher. The
demonstration explicitly selects the repository's example rules through
`AGENT_SCHEDULER_RULES`, leaving your active configuration untouched.

The real Codex descendant regression obtains exclusivity while the root is gone
and its child still holds inherited output handles (ordinary pipes/PTY and
restricted pipes). Restricted PTY also checks root release, allowing Codex's
existing ConPTY teardown to end the child. Unit tests hold release acknowledgements
to verify asynchronous waiting, cancellation and disconnect handling; process tests
check output draining.
