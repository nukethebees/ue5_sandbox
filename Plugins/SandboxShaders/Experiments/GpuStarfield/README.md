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
per-star brightness and subtle temperature variation. There is no tick and no per-star work after
generation.

The additive translucent material retains the normal scene depth test but does not write scene
depth. Opaque geometry therefore occludes stars while the background renderer does not contaminate
gameplay depth. The prototype does not force stars to the far plane: their logical radius controls
depth, so geometry farther away than a star can still render behind it.

## Controls

- `star_count` and `random_seed` regenerate and reupload the immutable star set.
- `starfield_scale` is the primary distance control. At `1`, the logical shell radius is 1,000 km
  and the base billboard diameter is 500 m. Scaling both together preserves angular star size.
- `star_size_multiplier`, `global_brightness`, and `parallax_strength` are advanced shader controls
  and update without rebuilding star data.
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
to make its stronger translational parallax identifiable during this experiment.

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
captures across three repeats, with 60 warmup frames followed by 180 measured frames per capture.
The CTest wrapper validates that enabling the starfield adds exactly one translucency draw and two
rendered primitives per star, then writes raw captures and consolidated CSV/Markdown reports below
`Saved/Benchmarks/GpuStarfield`. `latest.txt` identifies the most recent successful run.

The following Development results were measured on the same RTX 5090 and Ryzen 9 9950X3D system.
Values are medians of the three paired enabled-minus-disabled capture medians. The direct submission
scope measures `GetDynamicMeshElements`, which UE schedules on a worker in this configuration.

| Stars | Whole GT delta ms | Whole RT delta ms | Submit CPU ms | Whole GPU delta ms | Translucency GPU delta ms | Draw delta |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 10,000 | 0.0725 | 0.1528 | 0.0073 | 0.0743 | 0.0167 | 1 |
| 100,000 | 0.0038 | -0.0020 | 0.0060 | 0.1092 | 0.0607 | 1 |
| 1,000,000 | -0.0015 | -0.0161 | 0.0060 | 0.5625 | 0.5010 | 1 |

The whole-frame CPU deltas are small enough to remain sensitive to ordinary frame-to-frame
scheduling noise. The directly instrumented mesh submission remains below 0.01 ms and does not
scale with star count. GPU work scales with the number of unculled quads, reaching approximately
0.56 ms total and 0.50 ms in translucency at one million stars. Adding continuous depth, magnitude,
and colour to the existing record did not increase its 32-byte size or its one-draw architecture;
the measured GPU result remained within run-to-run variation of the single-depth implementation.
The subpixel footprint and energy correction also remained within that variation.

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
4. **Bright-star shape.** Give only the brightest population an optional compact procedural cross
   or diffraction spike without adding a texture or draw submission.
5. **Camera-motion benchmark.** Add a deterministic translation path to the automated A/B benchmark
   to validate parallax stability, bounds behavior, and motion cost.
6. **Coarse GPU culling, if justified.** Evaluate frustum and projected-size culling only after
   representative hardware and resolution measurements show that unculled quads need it.
