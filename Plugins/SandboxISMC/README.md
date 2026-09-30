# SandboxISMC

SandboxISMC is the custom instanced-mesh rendering experiment and its paired engine-ISMC comparison.
Use the demo level and `agent-task benchmark sandbox-ismc` to evaluate a specific update workload.

`--seconds` sets measured wall-clock time, excluding warmup. Set `--warmup-seconds` for a
timed warmup; sampling begins after both that duration and any `--warmup-updates` have elapsed.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the renderer/data flow, performance boundaries, and
comparison constraints.
