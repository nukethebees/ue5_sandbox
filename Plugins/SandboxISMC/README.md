# SandboxISMC

SandboxISMC is the custom instanced-mesh rendering experiment and its paired engine-ISMC comparison.
Use the demo level and repository benchmark workflow to evaluate a specific update workload.

`IOJ_ISMC_BENCHMARK_SECONDS` sets measured wall-clock time, excluding warmup. Set
`IOJ_ISMC_BENCHMARK_WARMUP_SECONDS` for a timed warmup; sampling begins after both that duration
and any `IOJ_ISMC_BENCHMARK_WARMUP_UPDATES` have elapsed.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the renderer/data flow, performance boundaries, and
comparison constraints.
