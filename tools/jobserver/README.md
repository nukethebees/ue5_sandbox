# NukeTheBees Jobserver

The jobserver is the local Windows scheduler and process supervisor used to coordinate expensive
development work across repository worktrees.

Install its canonical binaries with:

```powershell
cmake --preset native
cmake --build --preset native --target install-jobserver
```

Use `jobserver status`, `jobserver history`, and `jobserver logs <job-id>` to inspect work. Submit
ordinary work with `jobserver run`, exclusive benchmark work with `--exclusive machine` and
`--exclusive benchmark`, and integration transactions with `jobserver lease`.

Use `jobserver process-owner <pid>` to determine whether a process belongs to the current
worktree, then use `jobserver kill-owned` only when it reports `SafeKill yes`.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the daemon/client model, resources, leases, protocol, and
failure behaviour.
