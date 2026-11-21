# Procedural Space / Energy Field

Generate an animated HDR star, nebula, and plasma field on the GPU. Open the
[shader showcase](../../README.md) and select the blue/magenta panel on the right. Tune resolution,
speed, warp, star density, plasma intensity, and colours on `ASpaceEnergyFieldExperimentActor`.

Each tick, `FSpaceEnergyFieldCS` dispatches in 8×8 groups to a transient `RGBA16f` render target
(default 512×512). An unlit material samples it for emission. Find the shader in
`Shaders/Private/SpaceEnergyField/SpaceEnergyField.usf` and the noise helpers in
`Shaders/Common/ProceduralNoise.ush`. Use an SM5-capable RHI. NullRHI skips the dispatch.
