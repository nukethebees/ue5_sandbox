# SandboxISMC architecture

The component accepts dense bulk snapshots for one mesh. Producers retain position, quaternion
rotation and scale columns; chunk writers pack four float4 rows (64 bytes per instance) plus
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
transformed local AABBs using the packed scaled basis. Caller-supplied local bounds skip this work;
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
instanced float4 rows and null lightmap stream. Requests use resolved section materials, section
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

Future work can investigate root-relative int16 XYZ, quat32 and three lispb fixed-point 8-bit scale
components toward a 16-byte GPU stride. Multi-mesh rendering and native render preparation remain
separate later work; none of those representations or ownership changes are introduced here.

See [README.md](README.md) for the experiment entry point.
