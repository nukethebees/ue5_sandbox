# Procedural Space / Energy Field

This experiment generates an animated HDR space backdrop entirely on the GPU. It demonstrates a
real Unreal global compute shader and Render Dependency Graph dispatch rather than a material-only
effect.

## Pipeline

`Shaders/Private/SpaceEnergyField/SpaceEnergyField.usf` defines `FSpaceEnergyFieldCS`. It combines
hashed star cells, domain-warped five-octave value noise, nebula colour ramps, and narrow animated
plasma filaments. Shared noise functions live in `Shaders/Common/ProceduralNoise.ush`.

`ASpaceEnergyFieldExperimentActor` owns a transient UAV-capable `RGBA16f` render target and a plane.
Each editor/runtime tick snapshots its `FSpaceEnergyFieldSettings`, then enqueues a render command.
The render thread registers the target as an external RDG texture, dispatches the global shader in
8x8 groups, and returns the texture to SRV access. A minimal unlit material samples the result and
places it in Emissive Color.

The editable settings control resolution, animation speed, domain-warp scale and strength, star
density/intensity, plasma intensity, and the two nebula colours. These become shader parameters on
each dispatch; changing them never rewrites or recompiles the HLSL.

## Viewing it

Open `/SandboxShaders/Showcase/SandboxShaders_Showcase`, or choose **Window > Sandbox Shaders >
Open Shader Showcase**. The large blue/magenta panel is on the right. It runs in the editor viewport,
PIE, and standalone builds.

## Limitations

- The default 512x512 target is regenerated every tick and is intentionally not cached or adaptive.
- The experiment requires an SM5-capable RHI and skips dispatch under NullRHI.
- There is no temporal accumulation, readback, streaming, or production lifetime manager.
