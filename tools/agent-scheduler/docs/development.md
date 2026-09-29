# Installation and development

The maintainer runs `agent-task install-central-tools` to build/install the pinned modified Codex,
its code-mode host, and jobserver. Agents report missing components rather than installing them.
Canonical binaries live under `%LOCALAPPDATA%\NukeTheBees\agent-codex\bin`.
Close modified Codex sessions before updating. The launcher forwards normal Codex arguments;
security and sandbox settings remain Codex's responsibility.

Set `[tools.codex].build_profile` in `ioj.toml` to `debug`, `release-no-lto`, or `release`.
The standalone installer accepts a configuration override and a private staging root for build validation;
only the canonical installation can connect to production admission.
The build reuses the pinned upstream package builder's verified V8 artifacts in `.local/codex-v8`.

## Source boundary

`Prepare-Upstream.ps1` prepares the revision in `upstream-revision.txt` under `.local/codex-upstream`
and applies `codex.patch`. The scheduler crate owns transport, policy, ticket state, pseudo-command
parsing, and logical lifetime. The production patch adds its dependency, initializes once in session
startup, and wraps `handle_any_tool` for `exec_command` in `core/src/tools/registry.rs`.
There are no changes to PTY, sandbox backends, process management, or spawn/retry infrastructure.

## Focused validation

```powershell
cmake -S tools/agent-scheduler -B .local/scheduler-build -G Ninja
cmake --build .local/scheduler-build --target scheduler-unit-tests
cmake --build .local/scheduler-build --target codex-scheduler
cmake --build .local/scheduler-build --target codex-retry-test
```

Scheduler tests use in-memory framed peers and exercise rules, missing tickets, queued/granted execution,
single-ticket rejection, clearing, cancellation, and fail-closed disconnects.
The pinned Codex regression wraps its existing sandbox-denied/approved-retry test in one scheduler call.
Its mock admission peer asserts exactly one request and one release; no retry state reaches scheduling.
The test-only feature is absent from installed builds. Jobserver tests independently cover FIFO and
Windows identity restrictions. No live installation, model service, Unreal build, or process-survival
test is needed.
