# Jobserver

A cooperative, per-user Windows FIFO admission gate for modified Codex.
Shared tickets overlap. An exclusive ticket waits for earlier work and blocks later shared work.

Agents request tickets inside [modified Codex](../agent-scheduler/README.md), then run ordinary commands.
The server never executes programs, owns processes, or configures child environments.

Human diagnostics:

```powershell
jobserver status
jobserver trace
jobserver doctor
jobserver --version
```

The maintainer installs the daemon and CLI with `agent-task install-central-tools`.
`jobserver start` starts the registered logon task; `jobserver shutdown` succeeds only when no tickets remain.
Missing components require maintainer action. There is no automatic recovery or installation.

See [architecture](ARCHITECTURE.md) for protocol, identity checks, and accepted limitations.
