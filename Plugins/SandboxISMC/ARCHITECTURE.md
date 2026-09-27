# SandboxISMC architecture

The component accepts dense bulk snapshots for one mesh. Producers retain position, quaternion
rotation and scale columns; chunk writers pack a 16-byte render transform plus
optional custom floats. It owns no gameplay, collision, physics or navigation state. Ray tracing
remains disabled. Negative scale on any instance axis is unsupported and checked before packing;
component-level reverse culling remains supported.

## Assets and rendering

Assets must retain initialized, non-empty conventional LOD0 vertex/index resources. LOD0 must be
inlined, non-optional and resident: disable mesh LOD streaming and retain LOD0 in platform cooking
settings. Unsupported assets log a warning and do not create a proxy. There is no streaming retry,
LOD selection, per-instance culling or motion-vector path. Nanite-only assets without conventional
LOD0 are unsupported. Live mesh rebuilds require component re-registration and a new snapshot.

Mesh bounds synchronize on registration, assignment (including assignment of the same mesh), and
before packing. Mesh replacement invalidates the previous snapshot. Automatic bounds union
transformed local AABBs using source position, normalized quaternion and scale. Both automatic
and caller-supplied source bounds receive a conservative codec expansion once per snapshot;
callers must include any material vertex displacement. Unchanged bounds still submit dynamic data
but do not dirty the primitive transform. Unreal's normal transform update refreshes world bounds.

Materials resolve to an instancing-compatible surface material or the default surface material
before deriving relevance, WPO state, occlusion or PSO parameters. Mesh sections retain their shadow
enablement beneath the component's dynamic shadow flag. Production laser and fighter components
use Movable mobility intentionally: all batches are dynamically collected, with no static mesh
draw-list or baked lighting implementation. Movable also avoids treating animated batches as static
shadow-cache content. Component attachment transforms remain Unreal's responsibility.

Runtime vertex streams bind during vertex-factory RHI initialization, after the mesh's queued
resource initialization, so newly loaded or duplicated meshes cannot cache premature null SRVs.
Unavailable LOD0 RHI resources log a diagnostic and suppress drawing.

PSO precaching supplies an explicit LOD0 declaration through `CollectPSOPrecacheData`. Mesh stream
binding and declaration construction are shared with the runtime vertex factory, including its
four instanced uint attributes and null lightmap stream. Requests use resolved section materials, section
shadow state and component reverse culling. The normal proxy-delay policy is honoured. This is
component precaching; the generic local-VF declaration is not used for these requests.

## Staging and capacity

Three retained CPU staging slots rotate through previous/current/next roles. Pending, unqueued
snapshots may be superseded. A queued upload captures the staging owner until the render thread
has copied it; initial proxy uploads retain that same owner. `in_flight` protects CPU slot reuse.
If the next slot is still busy, the existing `FlushRenderingCommands` fallback remains. It is not
a GPU fence and does not prove GPU buffer reuse is stall-free.

Transform and custom-data GPU buffers retain power-of-two capacity and upload the live prefix
with lock/copy/unlock. Clear does not shrink buffers on a surviving proxy. Proxy recreation starts
new GPU allocations. `reserve_instances` sets a non-shrinking CPU capacity floor applied when each
slot next becomes writable, using the configured custom-data stride; it neither submits instances
nor forces a render-thread wait. Chunk bounds also retain capacity. There is no trim operation.

Buffer flags and RHI upload strategy are unchanged. A later measured experiment can compare
dynamic/ring/upload buffers, including GPU-side synchronization and allocation spikes separately
from CPU staging waits. Do not infer transfer bandwidth from lock/copy/unlock CPU timing.

## Measurements and regression coverage

`build_ms` measures producer packing and bounds reduction after staging acquisition. The lab's
`api` timing includes the complete `set_instances` call; `submit_ms` measures the later game-thread
render-command enqueue. Staging waits have lifetime count/time totals. `submitted_bytes` describes
the latest producer snapshot; `uploaded_bytes` describes the latest render-thread-consumed snapshot,
with lifetime `uploads` and `total_uploaded_bytes` counters. Render-thread fields are asynchronous
observations, not an atomic snapshot matched to the latest producer call. `upload_ms` is render-thread
allocation/lock/copy/unlock CPU time, not GPU transfer time. Lab CSV `last_render_*` samples can repeat
or skip uploads and must not be summed as transfer totals. Existing frame/GPU timing covers the whole
benchmark scene.

The paired comparison keeps mesh, material, transforms, mobility, visibility and movement equivalent
while retaining engine ISMC's normal GPU Scene/culling costs. CSV and Insights results describe that
workload, not a universal renderer ranking. Run focused unit/producer tests with CTest
`SandboxISMC.UnitTests` and real proxy, mesh-batch and pixel tests with `SandboxISMC.RenderTests`.
The latter verifies fallback relevance, section shadows, shared primitive uniforms, matching PSO
declarations, LOD restrictions, frustum visibility and queued update/recreation/destruction lifetimes.

## Packed transform ABI

`ml::sandbox_ismc::PackedTransform` (also `FSandboxISMCRenderInstance`) is little endian,
standard layout, trivially copyable, 16 bytes, alignment 4. Compile-time assertions fix every offset.
The `sandbox_ismc_render` module in `lispb/schema/sandbox_ismc.lispb` owns `PackedTransform`,
`Quat32` storage and `Scale8`, emitted into `sandbox/core/sandbox_ismc_render.h`. In the memory
planner, refresh `sandbox-code` and select `sandbox_ismc_render / PackedTransform` to inspect the
record and its nested fields. Encoding and decoding remain in handwritten C++ and HLSL.

| Byte offset | Storage | Meaning |
| --- | --- | --- |
| 0, 2, 4 | three signed int16 | XYZ position offsets |
| 6 | uint16 | reserved, zero |
| 8 | uint32 | smallest-three quaternion |
| 12, 13, 14 | three lispb `Scale8` | independent unsigned Q5.3 scales |
| 15 | uint8 | reserved, zero |

Every non-empty submission supplies a finite, ordered **position domain**, separately from optional
**render bounds**. The domain must contain every submitted position. Its centre is rounded to the
fixed 16-UU grid to choose the snapshot root. Each offset is `floor((position-root)/16 + 0.5)`;
ties go toward positive infinity. Supported offsets are -32767 through +32767, giving +/-524272 UU
(5.24272 km) per axis and at most 8 UU (8 cm) rounding error per axis. The vector error is at most
`8*sqrt(3)` UU. Invalid domains and numeric overflow terminate with diagnostics; domain membership is a slow-check caller contract;
there is no clamping, adaptive precision or first-instance fallback. Empty submissions ignore the domain.

The originally proposed 1-UU quantum cannot cover authored production populations.
`LevelScripts/Libraries/benchmark-fleet.scm` uses 240000-UU front separation and 24000-UU row
spacing. `BenchmarkFleet_10.scm` requests 160 capitals per team in 16 columns: the opposing rear
rows are at Y=-336000 and +336000, and their fighters share the rendering component. An 8-UU
quantum reaches only +/-262136 UU. Sixteen is the smallest power-of-two quantum covering this
population. No fixed domain guarantees arbitrary future flight distances; submissions beyond the
supported span remain errors.

The root and quantum occupy one float4 in each staging snapshot and a VF uniform on the render
thread. Upload compares and publishes this metadata with that snapshot's buffer. Initial proxy
creation consumes the same staging metadata; recreation retains the latest complete snapshot.
Metadata is never read from mutable component state on the render thread and is not charged as
per-instance bytes.

Scale uses existing lispb fixed-point storage, unsigned 8 bits with 3 fractional bits and nearest-even
rounding: 0..31.875, step 0.125, error at most 0.0625. Zero and one are exact. All production
fighter/laser callers currently submit unit scale. Non-finite, negative and excessive scale fail.
The schema lives in `lispb/schema/sandbox_ismc.lispb`; generated code must not be edited manually.
The render encoder multiplies float input by 8, truncates a range-checked integer, and applies
nearest-even from the fractional remainder. It constructs the generated `Scale8::from_raw`;
no general double fixed-point machinery runs per instance. Tests compare every rounding boundary
and its immediate float neighbours with the generated reference encoder.

Quaternion bits 0..1 are the omitted component index (X=0, Y=1, Z=2, W=3). Bits 2..11, 12..21,
and 22..31 store the other components in XYZW order. The render path requires finite normalized
input (squared length within 1e-4 of one), checked with `checkfSlow`/native debug assertions. Select the
largest absolute component (first index wins ties), negate if that component is negative, and map
the remaining components from [-1/sqrt(2), +1/sqrt(2)] to unsigned codes 0..1023, rounding to nearest.
Decode with the inverse affine mapping and reconstruct the omitted positive component as
`sqrt(max(0, 1-dot(stored,stored)))`. Clamp the mapped float before unsigned conversion.
The render encoder does not normalize or take a square root. A separate reference/debug entry point
normalizes arbitrary nonzero input and rejects squared lengths below 1e-12.

The self-contained implementation follows the established technique described by
[Glenn Fiedler](https://gafferongames.com/post/snapshot_compression/), with
[James Preiss's MIT implementation](https://github.com/jpreiss/quatcompress/blob/master/quatcompress.h)
as an algorithmic reference; its bit layout is defined here independently. No dependency or new
lispb quaternion semantics is introduced. The deterministic 100000-sample native test observed
0.233091599 degrees maximum and 0.088502779 degrees RMS error. A conservative 0.3-degree budget
covers this 10/10/10 encoding (component rounding error <=1/(sqrt(2)*1023), including reconstruction).
These are lossy rotations: identity and exact axis rotations need not decode bit-exactly.

The VF reads four `VET_UInt` attributes 8..11 at byte offsets 0/4/8/12, stride 16, and sign-extends
position lanes explicitly in HLSL. `PackedTransform.ush` reconstructs position and a scaled basis
in vertex shader registers. The GPU buffer remains compressed; there is no expanded persistent
buffer. Decoding is repeated per processed vertex, so reduced bandwidth may trade against ALU.

`SandboxISMCVertexFactory.ush` is a small wrapper around the actual engine
`/Engine/Private/LocalVertexFactory.ush`. UE 5.8 has no suitable instance-input hook, so install
our optional 18-line engine hook once per engine checkout:

```powershell
python Plugins/SandboxISMC/Tools/apply_local_vertex_factory_hook.py C:/dev/UE5.8.0
python Plugins/SandboxISMC/Tools/apply_local_vertex_factory_hook.py C:/dev/UE5.8.0 --check
```

The idempotent script validates three declaration blocks and one decoder before writing. It wraps
existing declarations in an optional macro and adds a generated-include hook after the engine's
instance-input type/helpers. Default engine VFs retain their existing declarations and decoder.
The plugin supplies four integer attributes and `PackedInstanceInput.ush` through the normal shader
compiler virtual-include map. The wrapper also exposes that adapter to UE's dependency/uniform
scanner; implementation is emitted only at the engine hook. All material, depth, position+normal,
custom-data and tangent logic remains engine-owned. An unpatched engine fails shader compilation
with an explicit hook diagnostic. On engine upgrades, review these four hook sites and rerun render
contracts/pixel tests. No full engine shader is distributed. Runtime and PSO declarations still share
one builder.

Both bounds paths now describe **source geometry**. Automatic bounds use the source transform's
ordinary mesh AABB, without decoding anything just encoded. Supplied bounds skip all per-instance
bounds work. After the source boxes are reduced, the component expands once per snapshot by
`8 + mesh_radius * (0.0625 + 0.006 * 31.875)` on each axis, plus a float evaluation margin.
Here `mesh_radius = length(abs(mesh_bounds_origin) + mesh_bounds_extent)` bounds every mesh vertex's
distance from the transform origin, including off-centre meshes. The scale term bounds
`R_decoded * (S_decoded-S_source) * vertex`; the rotation term bounds
`(R_decoded-R_source) * S_source * vertex`. A 0.006 chord allowance exceeds the 0.3-degree codec
budget and includes the normalized-input tolerance. This deliberately uses the supported maximum
scale, avoiding a supplied-bounds scale scan. Callers still include material displacement. A position
domain alone is never a primitive culling bound.

Position encoding uses a constant reciprocal and guarded truncation with explicit remainder/tie
handling. Double subtraction remains necessary: `root=262144`, `position=nextafter(8,0)` crosses
the intended tie if subtraction is performed in float. There is no division or `floor` in the
per-instance path. Range guards reject NaN/overflow before integer conversion in every build.

Fighter/laser rotations come from Unreal `FRotator3f::Quaternion()`, and both submit unit scale.
Their domain min/max accumulation is fused into the already-required visibility filtering pass;
there is no separate domain traversal. No suitable cached presentation-position bounds were found.
The benchmark's generated-layout domain is prepared outside measured updates. Domain discovery cost
is therefore excluded consistently from both Phase 1 and packed benchmark cases.

Native tests cover bit patterns, signed lanes, field offsets, quaternion error, domain selection,
position range/rounding, and generated scale rejection/rounding. Component tests cover decoded
bounds and source ownership. Pixel tests compare the actual custom-VF silhouettes against engine
ISMC instances transformed by the CPU decoder, using nonuniform scales, arbitrary rotations,
positive/negative offsets, rounding boundaries, changing distant roots, and proxy recreation.

Multi-mesh rendering, shared groups, native render preparation, culling, LOD, motion vectors and
upload-buffer redesign remain outside this experiment.

## Initial reference-encoder measurements (2026-09-27)

These historical results describe the first correctness-oriented encoder, before the optimization
pass below. They demonstrate why the encoder needed profiling; they are not the final recommendation.

The matched comparison uses the merged Phase 1 renderer from `ce4dda761` (unchanged in the rebased
`c58aa5fce` source) and the initial packed reference encoder. Both were built with the same Development
editor configuration, MSVC 14.50 and LLVM 24 native dependencies. The only baseline benchmark change
was accepting the supplied-bounds command-line switch. Native simulation representation was unchanged.

Each case used 5 seconds warmup followed by 60 measured seconds: 40000 engine cubes, custom-only,
all visible, 100% updated, no custom data, shadows off, identical map/camera, 1250x452 offscreen PIE
viewport, D3D12/SM6, RTX 5090, driver 610.88. The jobserver held the exclusive machine resource;
no coordinated builds ran during measurements. Initial baseline trials had substantial frame-time
variation and a different viewport, so they were repeated and excluded from this comparison.

Times below are **median / p95 milliseconds**. Submitted/uploaded bytes are transform payload only.

| Metric | Phase 1 automatic | Packed automatic | Phase 1 supplied | Packed supplied |
| --- | ---: | ---: | ---: | ---: |
| Measured frames | 7199 | 7180 | 7198 | 7199 |
| Custom total update | 0.2117 / 0.2572 | 0.6370 / 0.7605 | 0.1555 / 0.1983 | 0.5485 / 0.6007 |
| Build/packing and bounds | 0.2070 / 0.2522 | 0.6307 / 0.7548 | 0.1497 / 0.1930 | 0.5437 / 0.5956 |
| Render-thread upload CPU | 0.1188 / 0.1353 | 0.0355 / 0.0517 | 0.1252 / 0.1604 | 0.0318 / 0.0421 |
| Submitted bytes/snapshot | 2560000 | 640000 | 2560000 | 640000 |
| Uploaded bytes/snapshot | 2560000 | 640000 | 2560000 | 640000 |
| Staging waits | 0 | 0 | 0 | 0 |
| Whole-scene GPU | 1.3687 / 1.5675 | 1.3807 / 1.6414 | 1.3588 / 1.5832 | 1.3454 / 1.5690 |
| Frame | 8.3335 / 8.3335 | 8.3335 / 8.3336 | 8.3335 / 8.3335 | 8.3335 / 8.3335 |

Automatic build median increases 3.05x; supplied-bounds build median increases 3.63x. Supplying
bounds saves about 0.087 ms in the packed run, but the regression remains in packing itself.
The snapshot format adds position checks/rounding, quaternion encoding and three generated fixed-point
encodes; automatic bounds also decode the stored transform. This experiment does not isolate costs
within those operations. Upload median falls by 70%/75% (automatic/supplied), saving about 0.083/0.093 ms
on the render thread against about 0.424/0.394 ms additional producer build time. These are different
threads and asynchronously observed medians, not additive critical-path timings.

Whole-scene GPU medians differ by less than 1% on this cube workload. Frame timing remains effectively
120 Hz despite the benchmark's frame-cap settings, so it demonstrates no end-to-end frame improvement.
These GPU numbers are indicative, not an isolated VF timing or evidence about high-vertex fighter
meshes. No production fighter/laser performance or subjective visual-quality claim is made: the
existing focused benchmark fixes the cube mesh, and a new production benchmark suite was not added.
The automated pixel tests detected no CPU/GPU decode mismatch; quantization error remains as specified
above and may produce movement steps. The initial copied shader has since been replaced by the small engine hook described above.

Validation passed: regenerated lispb output and consistency checks; native core tests including the
seven packing tests and the compile contract; 22 focused Unreal unit/component/producer/render
contract/lifecycle/pixel tests; DebugGame and Development editor builds; formatting and diff review.
The full native tidy audit found one test printing-style diagnostic, corrected and followed by a clean
core audit. No sanitizer build was requested or run. Raw CSVs and editor logs are retained locally
under `.local/benchmarks/sandbox-ismc-packed/` in `phase1-*-matched` and `phase2-*` directories.

See [README.md](README.md) for the experiment entry point.

## Specialized scalar encoder profile

Temporary native CMake microbenchmark, LLVM 24 `/O2 /fp:precise`, 40000 deterministic randomized
normalized quaternions/positions/scales, 101 iterations (medians in ms). These isolated single-thread
figures diagnose the codec; they are not additive or directly comparable to the parallel UE benchmark.

| Operation | Reference encoder | Specialized encoder |
| --- | ---: | ---: |
| Position XYZ | 0.1072 | 0.1035 |
| Quat32 | 1.0530 | 0.5934 |
| Scale XYZ | 1.6831 | 0.1348 |
| Domain membership alone | 0.0296 | 0.0303 |
| Packed stores alone | 0.0140 | 0.0147 |
| Complete codec | 2.7958 | 0.7247 |

The dominant avoidable cost was general fixed-point conversion. Quaternion normalization and
loop/index bookkeeping were also removed. Supplied bounds have no per-instance bounds work;
automatic bounds no longer decode position, quaternion or scale immediately after packing.

## Optimized matched comparison and decision (2026-09-27)

**Recommendation: retain Phase 1 for production.** Keep this branch as the completed experiment,
not an integration candidate. The specialized encoder substantially improves on the reference
implementation, but it does not provide a compelling overall replacement for the production
automatic-bounds path. This is a result for this implementation/workload, not a claim that transform
compression is inherently too slow.

One new matched comparison used the same configuration and workload listed above, with the original
1250x452 viewport reset before each run. Each case had 5 seconds warmup and 60 seconds measurement,
under the jobserver's exclusive machine reservation. No builds, tests or static analysis ran alongside
these measurements. Phase 1 sources were temporarily installed from `c58aa5fce`, with only the existing
supplied-bounds CLI option added; the optimized sources were then restored byte-for-byte. The local
engine hook remained installed for both versions, with its ordinary LocalVF path unchanged for Phase 1.

Times are median / p95 milliseconds.

| Metric | Phase 1 automatic | Optimized automatic | Phase 1 supplied | Optimized supplied |
| --- | ---: | ---: | ---: | ---: |
| Measured frames | 7199 | 7199 | 7200 | 7199 |
| Custom total update | 0.2132 / 0.2519 | 0.3657 / 0.4264 | 0.1635 / 0.1981 | 0.2571 / 0.3059 |
| Build/packing and bounds | 0.2088 / 0.2477 | 0.3614 / 0.4222 | 0.1590 / 0.1933 | 0.2526 / 0.3009 |
| Render-thread upload CPU | 0.1148 / 0.1262 | 0.0312 / 0.0356 | 0.1213 / 0.1332 | 0.0327 / 0.0451 |
| Submitted bytes/snapshot | 2560000 | 640000 | 2560000 | 640000 |
| Uploaded bytes/snapshot | 2560000 | 640000 | 2560000 | 640000 |
| Staging waits | 0 | 0 | 0 | 0 |
| Whole-scene GPU | 1.3603 / 1.5921 | 1.3507 / 1.5622 | 1.3426 / 1.5812 | 1.3269 / 1.5460 |
| Frame | 8.3335 / 8.3335 | 8.3335 / 8.3335 | 8.3335 / 8.3335 | 8.3335 / 8.3335 |

Compared with the initial packed encoder, build medians improve 43% (0.6307 -> 0.3614 ms) with
automatic bounds and 54% (0.5437 -> 0.2526 ms) with supplied bounds. Against the fresh Phase 1 runs,
build remains 73% and 59% slower, respectively. The producer spends an additional 0.1526/0.0936 ms;
the render thread saves 0.0836/0.0886 ms. These are different threads and asynchronous metric samples;
their medians cannot be added to infer critical-path or frame improvement. Supplying bounds narrows
the tradeoff considerably, but current fighter/laser callers use automatic bounds.

GPU medians differ by about 0.7%/1.2% and frame pacing remains effectively 120 Hz. This supplies no
convincing GPU or end-to-end speedup, and the cube workload establishes nothing about decode ALU on
representative high-vertex fighter meshes. No new production benchmark suite was introduced.

The 75% transform staging/upload/storage reduction is real. At 40000 instances, live transform data
falls from 2.56 MB to 0.64 MB per snapshot (three live-sized staging copies: 7.68 MB to 1.92 MB), before
custom data and allocation slack. Root metadata remains 16 bytes per snapshot, not per instance.
The producer regression, lossy transform contract, and small engine-hook maintenance requirement
outweigh that benefit for this production replacement decision. No SIMD packer, alternate codec,
GPU pre-expansion, extra buffer, multi-mesh rendering or native render-preparation architecture was added.

Retained branch history was reconstructed from the shader-free native ABI commit. The final source
tree was preserved exactly, and the copied engine shader blob is absent from branch ancestry.
Only the plugin-owned wrapper/adapter/decoder and the surgical reapplication script remain in public
source. Superseded commits remain local recovery objects, not ancestors intended for publication.

The raw optimized and fresh baseline CSVs/editor logs remain under
`.local/benchmarks/sandbox-ismc-packed/{optimized-auto,optimized-supplied,phase1-auto-rerun,phase1-supplied-rerun}`.

Final validation of the optimized candidate passed: lispb regeneration/consistency checks; native
core tests (including all nine packing cases and the compile contract); 23 Unreal unit/component,
producer, render-contract, lifecycle and pixel tests; DebugGame and Development editor builds;
and the full 319-file native clang-tidy audit with no diagnostics. The final test/audit phase followed
the matched benchmark comparison. Formatting and the final scope/history diff were reviewed.
No sanitizer run was requested. Pixel tests found no CPU/GPU decode mismatch; no subjective
production visual-quality claim is made. Existing render-only restrictions and unsupported negative
scale remain unchanged; normalized quaternion input is now an explicit producer contract.
