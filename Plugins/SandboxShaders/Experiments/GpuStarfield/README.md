# GPU Starfield

This experiment tests a minimal custom scene renderer for deterministic, camera-relative stars. An
`AGpuStarfieldExperimentActor` owns one `UGpuStarfieldComponent`. The component generates one
immutable 32-byte record per star and hands a copy to its `FPrimitiveSceneProxy`; the proxy uploads
that array to a render-thread-owned structured buffer. Each record stores a unit direction, angular
size, brightness, relative depth, and warm-to-cool colour value.

Each visible view submits one indexed, instanced mesh batch. The vertex factory reuses one
four-vertex quad, indexes the structured buffer with the stereo-correct instance ID, scales the unit
direction to its relative logical depth, subtracts a scaled LWC camera-to-actor offset, and expands
the result along the view right/up axes. Billboard size scales with relative depth, preserving its
generated angular size while camera translation produces stronger motion in nearer populations.
The vertex shader also projects that size into pixels. It expands diameters below 1.5 pixels to a
stable raster footprint and applies inverse-area intensity compensation, preserving the original
star energy rather than making stabilized stars artificially brighter. The additive unlit material
applies a squared circular falloff using the interpolated quad UV and uses vertex colour for
per-star brightness and subtle temperature variation. For the brightest population, the vertex
factory can remap that same falloff into a compact radial core and procedural cross while modestly
expanding only those quads. There is no tick and no per-star work after generation.

The additive translucent material retains the normal scene depth test but does not write scene
depth. Opaque geometry therefore occludes stars while the background renderer does not contaminate
gameplay depth. The prototype does not force stars to the far plane: their logical radius controls
depth, so geometry farther away than a star can still render behind it.

## Controls

- `star_count` and `random_seed` regenerate and reupload the immutable star set.
- `galactic_band_strength` controls the fraction of stars concentrated around the actor's local
  equatorial plane. `0` exactly retains uniform spherical generation. Actor rotation orients the
  band without regenerating it.
- `starfield_scale` is the primary distance control. At `1`, the logical shell radius is 1,000 km
  and the base billboard diameter is 500 m. Scaling both together preserves angular star size.
- `star_size_multiplier`, `global_brightness`, and `parallax_strength` are advanced shader controls
  and update without rebuilding star data.
- `bright_star_shape_strength` controls the procedural cross on only the brightest stars. `0`
  retains the original circular falloff; the default `0.25` is intentionally subtle. This is also a
  dynamic shader control and does not rebuild star data.
- `galactic_band_width_degrees` controls the standard deviation of the generated band latitude.
  Changing it or band strength regenerates the immutable star set.
- `parallax_strength = 0` makes the logical field follow camera translation; `1` behaves like
  ordinary actor-anchored world geometry. The default `0.01` gives the base scale an effective
  apparent distance of approximately 100,000 km.

The showcase actor defaults to 10,000 stars, seed 1337, and a scale of `1`. The editor clamp permits
profiling up to one million stars. A standalone profiling run can override the placed actor without
resaving the map by passing `-GpuStarfieldCount=N`.

Generation uses continuous randomized depth within three deliberately sparse populations: 1% near
at `0.001-0.01` of the base radius, 14% middle at `0.03-0.2`, and 85% distant at `0.5-1.0`. These are
not separate renderer layers: they remain interleaved in one structured buffer and one instanced
draw. The distribution is mostly dim and neutral-white, with the near population kept bright enough
to make its stronger translational parallax identifiable during this experiment. Direction
generation mixes a uniform sphere with a truncated normal latitude distribution around the local
equator. The default places 65% of stars in a 15-degree band while retaining a uniform population
throughout the rest of the sky.

## UE 5.8 references

The implementation was checked against this checkout's current engine source rather than older API
examples:

- `Engine/Source/Runtime/Engine/Private/VectorFieldVisualization.cpp` and
  `Engine/Shaders/Private/VectorFieldVisualizationVertexFactory.ush` for the smallest material mesh
  batch/custom vertex-factory pair and stereo-correct `GetInstanceId` usage.
- `Engine/Plugins/Enterprise/LidarPointCloud/Source/LidarPointCloudRuntime/Private/Rendering` for
  proxy-owned render resources and vertex-factory lifetime.
- `Engine/Source/Runtime/Engine/Private/Particles/ParticleSpriteVertexFactory.cpp` and
  `Engine/Shaders/Private/ParticleSpriteVertexFactory.ush` for camera-facing view-axis expansion.
- `Engine/Source/Runtime/Engine/Private/Components/ArrowComponent.cpp` and
  `PaperSpriteComponent.cpp` for current dynamic `FMeshBatch` population and view relevance.
- `Engine/Source/Runtime/RHI/Public/RHIResources.h` and engine uses of
  `FRHIBufferCreateDesc::CreateStructured` for immutable structured-buffer/SRV creation.
- `Engine/Shaders/Private/Nanite/NaniteDataDecode.ush` for UE 5.8's current view-space-to-pixel
  projection scale.

UE 5.8's GPU-scene vertex-factory path requires `VF_GPUSCENE_GET_INTERMEDIATES`, and instanced stereo
requires converting `SV_InstanceID` through `GetInstanceId`. Camera/actor subtraction uses double-
float LWC helpers before demotion to translated float space. The material's particle-sprite usage
flag is only a shader-permutation gate for this private vertex factory; no particle system is used.

## Automated isolated benchmark

Run the Development offscreen A/B benchmark through CMake:

```text
cmake --workflow --preset gpu-starfield-benchmark
```

The benchmark uses the showcase camera at 1280x720 with deterministic timing, but hides and stops
the other placed showcase experiments. For each star count it alternates disabled and enabled
captures across three repeats in both stationary and moving-camera modes, with 60 warmup frames
followed by 180 measured frames per capture. The moving mode follows a deterministic curved path
covering 10,000 km. The actor verifies the camera completed at least 95% of that distance. The CTest
wrapper validates that enabling the starfield adds exactly one translucency draw and two rendered
primitives per star in both modes, and that every measured moving frame retains the full star
primitive count. It then writes raw captures and consolidated CSV/Markdown reports below
`Saved/Benchmarks/GpuStarfield`. `latest.txt` identifies the most recent successful run.

The following Development results were measured on the same RTX 5090 and Ryzen 9 9950X3D system.
Values are medians of the three paired enabled-minus-disabled capture medians. The direct submission
scope measures `GetDynamicMeshElements`, which UE schedules on a worker in this configuration.

| Stars | Camera | Whole GT delta ms | Whole RT delta ms | Submit CPU ms | Whole GPU delta ms | Translucency GPU delta ms | Draw delta |
| ---: | :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| 10,000 | Stationary | 0.0496 | 0.0661 | 0.0062 | 0.0656 | 0.0167 | 1 |
| 10,000 | Moving | 0.0331 | 0.0716 | 0.0058 | 0.0657 | 0.0169 | 1 |
| 100,000 | Stationary | 0.0252 | 0.0885 | 0.0060 | 0.1091 | 0.0608 | 1 |
| 100,000 | Moving | 0.0231 | 0.0450 | 0.0058 | 0.1104 | 0.0610 | 1 |
| 1,000,000 | Stationary | 0.0474 | 0.1158 | 0.0063 | 0.5584 | 0.5000 | 1 |
| 1,000,000 | Moving | 0.0335 | 0.0638 | 0.0059 | 0.5484 | 0.4997 | 1 |

The whole-frame CPU deltas are small enough to remain sensitive to ordinary frame-to-frame
scheduling noise. The directly instrumented mesh submission remains below 0.01 ms and does not
scale with star count. GPU work scales with the number of unculled quads, reaching approximately
0.56 ms total and 0.50 ms in translucency at one million stars. Adding continuous depth, magnitude,
and colour to the existing record did not increase its 32-byte size or its one-draw architecture;
the measured GPU result remained within run-to-run variation of the single-depth implementation.
The subpixel footprint, energy correction, and default subtle bright-star shape also remained within
that variation. Moving the camera across the 10,000-km path did not add measurable starfield GPU
cost or change its draw/primitive counts. Whole-frame render-thread differences remain noisy.

### Resolution and apparent-size matrix

Run the bounded stationary matrix separately:

```text
cmake --workflow --preset gpu-starfield-matrix
```

This reuses the same paired A/B capture and validation path for 1080p and 4K, 100,000 and one
million stars, and the default and 4x star-size multipliers. Each of the eight cases uses three
paired repeats. Unreal requires `-ForceRes` for these offscreen resolutions; the runner reads the
CSV metadata and fails the test if the actual render resolution does not match the requested one.

The following RTX 5090 results came from the verified run in `20260830_180144`:

| Resolution | Size | Stars | Submit CPU ms | GPU delta ms | Translucency GPU delta ms | Draw delta |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1920x1080 | 1x | 100,000 | 0.0061 | 0.1079 | 0.0609 | 1 |
| 1920x1080 | 1x | 1,000,000 | 0.0065 | 0.5665 | 0.4979 | 1 |
| 1920x1080 | 4x | 100,000 | 0.0061 | 0.1084 | 0.0611 | 1 |
| 1920x1080 | 4x | 1,000,000 | 0.0060 | 0.5719 | 0.4999 | 1 |
| 3840x2160 | 1x | 100,000 | 0.0061 | 0.1052 | 0.0606 | 1 |
| 3840x2160 | 1x | 1,000,000 | 0.0075 | 0.5742 | 0.4980 | 1 |
| 3840x2160 | 4x | 100,000 | 0.0067 | 0.1164 | 0.0612 | 1 |
| 3840x2160 | 4x | 1,000,000 | 0.0059 | 0.5782 | 0.5030 | 1 |

The small differences across resolution and size are comparable to run-to-run variation. For the
current tiny stabilized stars, this indicates that instance/vertex work and minimum footprints
dominate rather than resolution-scaled fill. Coarse GPU culling is therefore not justified by this
hardware result. Target-hardware measurements remain necessary before making a production choice.

## Initial whole-showcase profiling baseline

The architecture produces one draw submission per visible view at every tested count. Expected
star-buffer sizes are:

| Stars | Buffer bytes | Approximate MiB |
| ---: | ---: | ---: |
| 10,000 | 320,000 | 0.31 |
| 100,000 | 3,200,000 | 3.05 |
| 1,000,000 | 32,000,000 | 30.52 |

An offscreen 1280x720 D3D12 SM6 capture was made on an RTX 5090 with a Ryzen 9 9950X3D. Values below
are means of the final 30 steady frames in a 90-frame DebugGame editor-process CSV capture. They are
useful as an architecture sanity check and historical comparison, not as isolated starfield costs:
the level contains all showcase experiments, the render-thread and total-GPU columns include the
whole frame, and the translucency GPU bucket includes the showcase's other translucent effects.

| Stars | Game thread ms | Render thread ms | Total GPU ms | Translucency GPU ms |
| ---: | ---: | ---: | ---: | ---: |
| 10,000 | 1.11 | 3.08 | 1.08 | 0.031 |
| 100,000 | 1.04 | 2.73 | 0.99 | 0.073 |
| 1,000,000 | 1.15 | 2.92 | 1.48 | 0.535 |

The whole showcase reported 18 translucency draws at every count; the starfield contributes one of
them, once per visible view. Its render-thread submission cost was below the noise floor of this
short capture. One million unculled additive quads remained viable on this hardware, with the
expected GPU scaling rather than draw-call or game-thread scaling. Target-hardware viewport and
Unreal Insights captures are still required before drawing a SpaceGame production conclusion.

## Current limits and next step

The scene proxy uses UE 5.8's always-visible primitive path because its shader-relative star
positions do not fit stable actor-centred CPU bounds. This prevents primitive-level frustum,
distance, and occlusion culling after long camera travel without adding a game-thread camera-follow
tick. Normal material depth testing remains active, so opaque geometry still occludes star pixels.
The shell has no per-star frustum, size, or brightness culling and supports SM5-class desktop feature
levels only.

## Roadmap

Each step remains reviewable and usable before the next one begins. Unless measurements justify a
change, all steps preserve one component, one immutable star buffer, one draw per visible view, and
no per-star CPU work per frame.

1. **Camera-travel-safe bounds — complete.** The proxy uses UE 5.8's always-visible primitive path,
   preventing shader-relative stars from being culled after long camera travel without a CPU tick.
2. **Magnitude, colour, and depth distribution — complete.** Stars now use mostly dim magnitudes,
   a small identifiable bright population, subtle warm/cool variation, and continuous depth within
   sparse near, middle, and distant populations in the same 32-byte buffer and draw.
3. **Subpixel stability — complete.** The vertex shader now enforces a 1.5-pixel minimum diameter
   and compensates intensity by the inverse area increase, reducing unstable subpixel coverage
   without changing the star's intended energy, data, or draw count.
4. **Bright-star shape — complete.** The brightest population can now receive a configurable
   procedural cross around its radial core. Strength zero preserves the circular path, and the
   effect adds no texture, star data, or draw submission.
5. **Camera-motion benchmark — complete.** The automated benchmark now compares stationary and
   deterministic 10,000-km moving-camera A/B captures, verifies the camera travel, and checks the
   full one-draw primitive submission survives every measured moving frame.
6. **Galactic density band — complete.** Deterministic generation can now mix the uniform sky with
   a configurable soft band around the actor's local equator. Strength zero preserves the original
   distribution, and the feature does not change GPU data or per-frame work.
7. **Coarse GPU culling evaluation — complete.** The verified resolution/size matrix does not show
   a current need for it on the measured hardware. Revisit this only if representative target
   hardware or substantially different star appearance makes the unculled draw material.
