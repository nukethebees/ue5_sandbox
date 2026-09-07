# GPU-driven impact-spark rendering experiment

Date: 2026-09-07

## Decision

Keep the existing GPU-expansion renderer for impact sparks. Do not ship the procedural renderer from
this experiment.

The procedural fixed-group design successfully removed expanded per-particle storage from its data
model and removed the compute-expansion pass. In this benchmark it did not, however, produce a
measurable GPU or frame-time improvement over the existing renderer. It also required materially
more CPU-side lifetime and allocation machinery. Its apparent whole-game-thread improvement cannot
be attributed to the spark path with the timings collected here.

The procedural design remains worth revisiting if the expanded particle buffer becomes a meaningful
memory cost, or if a representative gameplay trace shows that expansion compute is significant.

## Scope and terminology

This experiment concerned impact sparks created from laser hits. It did not change the simulation or
rendering of in-flight laser projectiles.

The three measured modes were:

1. **CPU expansion:** the CPU generates a 64-byte particle record for every admitted spark and
   uploads those records.
2. **GPU expansion (production baseline):** the CPU uploads one 96-byte burst descriptor and a
   compute shader expands it into the persistent 64-byte particle buffer. This is the renderer that
   existed before the experiment and remains the selected implementation.
3. **Procedural fixed groups:** the CPU retains 96-byte burst descriptors in fixed groups and the
   vertex shader derives each particle directly from the descriptor, seed, and local particle index.
   There is no expanded particle record for the procedural data model.

Calling both modes 1 and 2 “GPU modes” is useful when discussing where particle generation happens,
but only mode 1 is the baseline. Mode 2 was the proposed experiment.

Impact inputs already contain location, emission direction, RGB colour, deterministic seed, and the
shared spark style. Team identity is not retained by this pipeline, so a team/style lookup table was
out of scope.

## Existing renderer

The production GPU-expansion path uploads a 96-byte descriptor for each submitted burst. The
descriptor contains the source data and allocation range required by the compute shader. A
64-thread workgroup expands each submitted burst into the persistent particle ring buffer.

Spark motion is already analytic after expansion. The vertex shader evaluates position, streak,
fade, thickness, and colour from initial position, velocity, acceleration, spawn time, lifetime, and
the current effect time. Both the CPU and GPU-expansion paths draw the configured particle capacity;
dead and distant records are rejected in the vertex shader.

At the default 16 sparks per burst:

- CPU expansion transfers 16 x 64 = 1,024 bytes per burst.
- GPU expansion transfers one 96-byte descriptor per burst in the normal, non-overflow case.
- GPU expansion therefore reduces normal submission data by 90.625% relative to CPU expansion.
- A 4-byte selected-particle index is additionally required only when an oversized submission must
  be sampled to fit the particle capacity.

Before this experiment, the expanded-particle buffer was persistent and preallocated, while the GPU
expansion input buffers were transient RDG buffers.

## First procedural prototype: compact particle references

The first prototype retained burst descriptors and generated an 8-byte reference for each active
particle. The reference identified the descriptor slot and the original particle index. A separate
8-byte active-work entry identified each descriptor and its output prefix, and a compute pass rebuilt
the reference buffer whenever bursts were added, expired, or recycled.

The design avoided mutable particle simulation state, but it was not the intended experiment. It
still materialised per-particle data and caused upload volume to scale with the active particle count
when the reference buffer was rebuilt. Observed uploads grew from approximately 10.4 KiB/frame at
100 active bursts to 409.6 KiB/frame at 50,000 active bursts. This was discarded.

The useful lesson is that variable burst sizes do not justify a per-particle indirection if small,
fixed groups and controlled overdraw are acceptable.

## Corrected procedural design: fixed groups

The corrected prototype used a configurable fixed group size, with 16 as the default. A logical
burst larger than the group size was split across multiple descriptors. For each rendered instance:

```text
descriptor slot      = instance ID / group size
local particle index = instance ID % group size
```

The descriptor retained the deterministic seed, original particle count, admitted particle count,
and first admitted index represented by that group. The vertex shader then:

1. Masked unused lanes in the last group.
2. Mapped a sampled-down admitted index back to its deterministic original particle index.
3. Ran the same integer hash, range sampling, and cone sampling rules as GPU expansion.
4. Evaluated particle motion and appearance analytically at the current effect time.
5. Rejected the particle if its generated lifetime had elapsed or it exceeded the draw distance.

This design had no active-work upload, per-particle reference buffer, reference-building compute
pass, expanded procedural particle records, mutable GPU simulation state, or GPU readback.

### Controlled overdraw

With group size 16, a burst draws `ceil(particle count / 16) * 16` instance slots. Only the final
group can contain unused lanes, so the group-size slack is at most 15 instances per logical burst.
The benchmark used exactly 16 sparks per burst and therefore had no group-size slack.

Descriptor slots were reused from a lowest-slot-first free heap. Drawing stopped at the occupied
high-water slot, and empty slots below it were masked by zero descriptors. Lowest-slot reuse quickly
fills holes under sustained churn without requiring a dense active-list upload. It does not guarantee
zero overdraw for arbitrary mixed lifetimes: a long-lived high slot can keep lower holes inside the
draw range.

An exactly dense descriptor array would remove those holes but would require moving descriptors and
repairing burst ownership when a group expires. The experiment did not add that complexity.

### Lifetime and reclamation

The GPU can stop drawing an individual spark from its analytically generated lifetime, but it cannot
unilaterally return the containing CPU-owned descriptor slot to the free list. The CPU may safely
reuse a complete group after the maximum possible lifetime in its descriptor has elapsed.

An early fixed-group implementation put burst records in insertion order and expired only from the
front. The benchmark prefilled long-lived bursts, then appended short-lived bursts. The long-lived
front record blocked reclamation of the later records; at 50,000-burst capacity, 44,100 expired
descriptors were eventually reclaimed in one frame, causing a 1.325-second submission spike.

The corrected implementation used:

- A minimum heap ordered by maximum expiry time.
- Stable burst-state slots with generation counters, allowing stale expiry entries to be ignored.
- A FIFO of generation-tagged burst references for deterministic oldest-complete-burst eviction
  under capacity pressure.
- A minimum heap of free descriptor slots to control the draw high-water mark.
- Coalescing when a descriptor was cleared and reused in the same frame.

After correction, the 50,000-burst run retained exactly 50,000 bursts/800,000 particles, reported no
drops or staging waits, and had a worst submission sample of 0.2312 ms.

## Public and renderer changes evaluated

The experimental implementation added:

- `sg.Sparks.Path`: `0=CPU`, `1=GPU expansion`, and `2=procedural`, defaulting to GPU expansion.
- The existing `sg.Sparks.CpuExpansion` as a legacy CPU override.
- Independent particle and burst capacities.
- A configurable procedural group size.
- A read-only metrics snapshot covering path, capacity, activity, submissions, draw instances,
  upload traffic, dispatches, persistent GPU memory, overwrites, drops, and staging waits.
- Persistent/preallocated GPU-expansion burst and overflow-selection buffers with retained CPU
  staging, replacing transient expansion inputs.
- A persistent procedural descriptor buffer.

Changing path cleared live spark state once so that records from one representation could not
reappear in another.

These changes were reverted after recording the experiment. They describe a possible future
implementation, not current public interfaces.

## Benchmark method

The offscreen Unreal automation benchmark rendered to a 1920 x 1080 `RGBA16f` target. It used:

- 16 sparks per burst.
- 100 newly submitted impacts per frame, or 1,600 new sparks per frame.
- 60 warm-up frames.
- 10 measured seconds per run.
- Target working sets of approximately 100, 1,000, 10,000, and 50,000 active bursts.
- Particle capacities of 1,600, 16,000, 160,000, and 800,000 respectively.
- One draw call in every mode.
- Persistent memory sufficient for hot-switching among all experimental paths.

The particle pool was prefilled so GPU draw cost represented the selected working set rather than a
mostly empty maximum-capacity buffer. Procedural prefill retained descriptors and used the normal
fixed-group draw path.

Each per-frame CSV recorded frame, game, render, RHI, GPU, and directly scoped submission time;
activity, submissions, uploads, updates, dispatches, draw work, capacities, persistent GPU memory,
overwrites, drops, and staging waits. Unreal Insights traces enabled CPU, GPU, frame, bookmark,
counter, stat, render-command, and RHI-command channels.

The benchmark ran modes sequentially and performed one timed run per mode/load combination. It was
adequate for large differences in directly scoped work, upload counts, dispatch counts, and memory,
but not for attributing small whole-frame timing differences.

## Corrected timed results

The following values are medians. `Submit` directly times burst queueing, expansion/allocation, and
submission. `Game` is Unreal's global `GGameThreadTime`; its limitations are discussed below.

| Active bursts | Path | Frame (ms) | Game (ms) | Submit (ms) | GPU (ms) | Upload/frame |
|---:|---|---:|---:|---:|---:|---:|
| 100 | CPU expansion | 8.3292 | 6.5398 | 0.3315 | 1.0722 | 102,400 B |
| 100 | GPU expansion | 8.3024 | 5.6785 | 0.1085 | 1.0654 | 9,600 B |
| 100 | Procedural | 8.2789 | 4.7294 | 0.1633 | 1.0482 | 9,600 B |
| 1,000 | CPU expansion | 8.2888 | 6.0473 | 0.3272 | 1.0713 | 102,400 B |
| 1,000 | GPU expansion | 8.3363 | 5.7787 | 0.1080 | 1.0795 | 9,600 B |
| 1,000 | Procedural | 8.1162 | 5.2338 | 0.1815 | 1.0653 | 9,600 B |
| 10,000 | CPU expansion | 8.2675 | 6.0146 | 0.3261 | 1.2340 | 102,400 B |
| 10,000 | GPU expansion | 8.3037 | 5.6849 | 0.1076 | 1.2470 | 9,600 B |
| 10,000 | Procedural | 8.2557 | 4.8966 | 0.1676 | 1.2312 | 9,600 B |
| 50,000 | CPU expansion | 8.2797 | 6.0665 | 0.3349 | 1.8792 | 102,400 B |
| 50,000 | GPU expansion | 8.3141 | 5.7456 | 0.1085 | 1.8905 | 9,600 B |
| 50,000 | Procedural | 8.2768 | 4.8249 | 0.1626 | 1.8875 | 9,600 B |

CPU upload remained constant because churn was fixed at 100 bursts/1,600 particles per frame. Both
GPU paths uploaded 100 x 96-byte descriptors per frame. Active working-set size affected draw and
GPU cost, not normal per-frame submission volume.

GPU expansion issued one compute dispatch containing 100 workgroups per frame. Procedural issued no
compute dispatch. All corrected procedural runs had the requested live burst/particle count, exact
draw capacity for the 16-spark workload, zero dropped bursts, and zero staging waits.

No meaningful GPU crossover was observed. The procedural GPU medians differed from GPU expansion by
only 0.0030--0.0172 ms, with procedural nominally lower in all four samples. That is too small to
distinguish from run variation. Removing expansion compute was approximately balanced by doing hash,
range, cone, and attribute generation for each quad vertex.

Direct procedural submission was 0.0541--0.0735 ms slower than GPU-expansion submission because it
maintained descriptor ownership, expiry, eviction, and free-slot heaps. It remained roughly half the
cost of CPU particle expansion.

Render-thread and RHI-thread medians were approximately 0.0005--0.0013 ms in this offscreen harness,
below a useful level for differentiating the paths.

## Why the game-thread result is inconclusive

The procedural runs reported `GGameThreadTime` medians 0.54--0.95 ms lower than GPU expansion, but
the directly scoped procedural submission was slower and median frame time remained near 8.3 ms for
all modes. The benchmark cannot causally assign the global game-thread difference to procedural
sparks.

The direct submission timer stopped immediately after queuing bursts and committing the spark
effect. `SendAllEndOfFrameUpdates()` and `CaptureScene()` ran after that timer. `GGameThreadTime` is a
global whole-engine-frame counter and may also refer to a different frame from the directly scoped
sample. It includes automation, world, capture, scheduling, and possible waiting costs unrelated to
the queue/commit work.

In addition, modes were run sequentially rather than in a repeated, randomised or alternating order.
Editor state, background work, shader/cache state, boost clocks, and thermal drift could therefore
be correlated with a path. The whole-game-thread numbers must not be used as evidence that the
procedural path is faster.

## Memory results

The experimental component allocated all mode-specific buffers simultaneously so paths could switch
at runtime. Consequently, every path reported the same persistent GPU allocation at each capacity:

| Particle capacity | Burst capacity | Experimental persistent GPU memory |
|---:|---:|---:|
| 1,600 | 16,384 | 3.10 MiB |
| 16,000 | 16,384 | 4.04 MiB |
| 160,000 | 16,384 | 13.38 MiB |
| 800,000 | 50,000 | 61.04 MiB |

Those figures include the 64-byte particle buffer, 4-byte overflow-selection buffer, 96-byte GPU
expansion descriptor buffer, and separate 96-byte procedural descriptor buffer. They do not
demonstrate the procedural model's potential production memory reduction.

At 800,000 particles/50,000 groups, path-specialised storage would be approximately:

- CPU expanded particle records: 48.83 MiB, excluding staging.
- GPU expansion particle, descriptor, and worst-case selection buffers: 56.46 MiB.
- Procedural 96-byte descriptors: 4.58 MiB, excluding allocator metadata and shared draw resources.

A production procedural path should allocate only its required buffers. Hot-switching experimental
paths should not be used to judge the final memory benefit.

## Correctness validation

The experiment used deterministic integer hashing and equivalent range/cone sampling in compute and
vertex paths. Visual parity meant equivalent deterministic distribution, motion, lifetime, colour,
and broad appearance, not bit-identical CPU/HLSL floating-point output.

Coverage included:

- Deterministic hashing and range generation.
- Oversized-burst deterministic sampling.
- Particle and burst capacity limits.
- Oldest-complete-burst eviction.
- Expiry and free-slot recycling.
- Mixed lifetimes where a newer short-lived burst expires before an older long-lived burst.
- Fixed-group splitting and controlled draw counts.
- Persistent expansion-input overflow.
- A broad HDR, movement, proxy-recreation, and expiry render test on all three paths.

The final implementation passed the formatting workflow, all 772 unit tests (one unrelated
platform-specific skip), and the three-path render test. All valid timed benchmark runs completed
without drops or staging waits.

## In-flight lasers

In-flight gameplay lasers remained on the separate SandboxISMC path throughout the experiment. Each
live laser rebuilds and uploads a 64-byte transform plus five custom floats, or 84 bytes per laser per
frame. Its GPU buffers grow persistently and its CPU staging is triple-buffered. This traffic was
documented for context but was not altered or benchmarked here.

## Lessons

- The existing GPU-expansion baseline already removes most CPU upload traffic: 96 bytes versus 1,024
  bytes for a normal 16-spark burst.
- A procedural renderer should map instance ID directly to a fixed descriptor group. A rebuilt
  per-particle reference buffer defeats much of the point.
- GPU lifetime rejection and CPU descriptor reclamation solve different problems. Safe reuse still
  needs CPU knowledge of a group's maximum possible expiry unless ownership is moved fully onto the
  GPU.
- A FIFO is valid for expiry only when maximum expiry is monotonic. Mixed lifetimes require an expiry
  heap, buckets, or another independently ordered structure.
- Lowest-slot reuse bounds common fragmentation without an active-list upload, but a high-water draw
  is not guaranteed to be dense under arbitrary lifetime patterns.
- Fixed groups exchange bounded vertex overdraw for simple address calculation and minimal upload.
- Removing a compute pass does not guarantee lower GPU time when its work moves into a vertex shader
  that executes once per quad vertex.
- Benchmark instrumentation must distinguish directly scoped work from global engine counters.
- A hot-switchable experiment is useful for correctness comparisons but can obscure the specialised
  memory footprint of each candidate.

## Conditions for revisiting

Reconsider procedural impact sparks if one or more of the following becomes true:

- The expanded particle buffer is a material part of the GPU memory budget.
- Representative gameplay traces show spark expansion compute or particle-buffer bandwidth as a
  measurable bottleneck.
- Spark counts grow substantially beyond the tested range.
- A procedural-only renderer can be evaluated without allocating the other paths' buffers.
- Burst styles become sufficiently regular that descriptor compression materially reduces bandwidth
  or storage.

Do not start by adding style, colour, or team lookup tables. First retain the same 96-byte descriptor
layout so any improvement isolates procedural generation. Compress descriptors only after measuring
a descriptor-bandwidth or memory bottleneck.

## Better follow-up benchmark

A future comparison should:

1. Build separate path-specialised configurations so memory and resource setup are representative.
2. Add named Insights scopes around queue/commit, end-of-frame component updates, capture/render
   submission, render-thread upload, compute expansion, and procedural draw work.
3. Repeat each path/load combination at least three times.
4. Interleave or randomise path order, preferably using an A/B/B/A sequence for focused comparisons.
5. Report confidence intervals or run-to-run ranges as well as per-frame medians and p95 values.
6. Use representative gameplay captures in addition to the synthetic offscreen workload.
7. Test non-multiple burst sizes and mixed lifetimes to quantify controlled overdraw and
   fragmentation.
8. Compare the dedicated memory footprints, including CPU staging, rather than a shared hot-switch
   allocation.

The focused GPU-expansion-versus-procedural matrix takes approximately four minutes per repetition
for four working-set sizes. The full three-path matrix takes approximately six and a half minutes per
repetition.
