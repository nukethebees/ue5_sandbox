# Single-allocation SoA architecture

The generated owner keeps all columns in one allocation. Each column starts at its required
alignment, stores its capacity elements, then the next column receives a fixed alignment-preserving
gap; no trailing gap is added. This preserves column synchronization while avoiding capacity-based
layout branches and retains compact views.

The current generated policy uses a 192-byte nominal gap, with each column aligned to
`max(64, alignof(Element))`. It was selected from fixed-CPU experiments after zero-gap layouts
showed severe wide-kernel slowdowns at selected power-of-two capacities. The result is evidence for
the measured schemas, kernels, compiler, and host—not a portable microarchitecture guarantee.

Benchmark runs compare layouts within one schema, pin the requested CPU, use fresh processes and
record layout identity, actual affinity, binaries, source revision, and individual process results.
Allocation and initialization remain outside the timed kernel. Correctness verifies alignment,
non-overlap, gap sentinels, nested views, and generated-owner equivalence before timing.

See [README.md](README.md) for the experiment entry point.
