# NukeTheBees Jobserver

The jobserver is a local Windows traffic gate. Ordinary commands and builds share the machine
freely. An exclusive benchmark waits for admitted work to finish, blocks later arrivals, then
runs alone. There are no compiler quotas or per-compiler launchers.

Agents use the persistent `jobserver broker` with framed JSON input and output. For one command:

```powershell
jobserver run --shared machine --name "Native build" -- cmake --build --preset native
jobserver run --exclusive machine --name "Measurement" -- benchmark.exe
jobserver status
jobserver trace --lease 123 --limit 100
```

The client executes locally and releases the lease when the root command exits. Persistent
compiler-server children do not hold leases. Closing a broker terminates its remaining session
processes and releases its leases through pipe disconnect.

The maintainer installs the canonical binaries with:

```powershell
cmake --preset native
cmake --build --preset native --target install-jobserver
```

Keep `%LOCALAPPDATA%\NukeTheBees\jobserver\bin` on PATH. `jobserver --version` reports the
source and protocol versions. Installation retains staged validation, drain/shutdown, startup
verification, and rollback; never copy over live binaries. Protocol 2 requires matching clients
and daemon. Agents do not update the canonical installation themselves.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the broker interface, FIFO rules, tracing, and failures.
