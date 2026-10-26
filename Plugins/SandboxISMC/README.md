# SandboxISMC

SandboxISMC provides the instanced-mesh renderer used by SpaceGame's fighters and lasers.
The optional [SandboxISMCLab](../SandboxISMCLab/README.md) plugin contains the demo
levels and `coj benchmark sandbox-ismc` comparison against engine ISMC. Enable it
and rebuild the editor before benchmarking.

`--seconds` sets measured wall-clock time, excluding warmup. Set `--warmup-seconds` for a
timed warmup; sampling begins after both that duration and any `--warmup-updates` have elapsed.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the renderer/data flow, performance boundaries, and
comparison constraints.
