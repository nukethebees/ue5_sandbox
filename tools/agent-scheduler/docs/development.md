# Installation and development

## Central installation

Update `agent-task` once with `. ./dev.ps1` followed by `install-agent-task`, then
run `agent-task install-central-tools`. This prepares the pinned source, builds
Codex and the scheduler client with the `ReleaseNoLTO` configuration, and installs them under
`%LOCALAPPDATA%\NukeTheBees\agent-codex\bin`. Close custom Codex sessions before updating.
This maintainer-only installer builds directly, without requesting a jobserver lease.

The `agent-codex.ps1` launcher forwards normal Codex arguments, selects the
unelevated backend, disables shell snapshots, and loads the installed scheduling
rules. Normal `codex` remains unchanged.

The [standalone installer](../../install/Install-AgentCodex.ps1) accepts
`-InstallRoot <private-root>` and `-Configuration Debug` for staging validation
without replacing installed tools.

The default uses [build-profile.toml](../build-profile.toml): Cargo's
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
local Responses events; no model service is contacted. Coverage includes FIFO
admission, cancellation/grant races, spawn failure, immediate ticket reuse,
daemon loss, independent security rejection, startup failure and a real
sandbox-denied write followed by an approved retry.

`-Only lifecycle` or `-Only leases` selects a focused group.
Use `-InstalledBin <install-root>/bin` to exercise the installed launcher and its
rules in the Codex integration demonstration.

The real Codex descendant regression obtains exclusivity while the root is gone
and its child still holds inherited output handles (ordinary pipes/PTY and
restricted pipes). Restricted PTY also checks root release, allowing Codex's
existing ConPTY teardown to end the child. Unit tests hold release acknowledgements
to verify asynchronous waiting, cancellation and disconnect handling; process tests
check output draining.
