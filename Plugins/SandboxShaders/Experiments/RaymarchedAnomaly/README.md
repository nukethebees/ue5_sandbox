# Raymarched Energy Anomaly

This experiment renders an animated signed-distance field entirely inside a material Custom
expression on a flat quad. The shader in
`Shaders/Private/RaymarchedAnomaly/RaymarchedAnomaly.ush` constructs a noisy sphere and rotating
torus, marches a local camera ray toward the SDF, derives normals with finite differences, and adds
surface bands, rim emission, and a cheap glow accumulated along the ray.

`ARaymarchedAnomalyExperimentActor` supplies an explicit animation clock and dynamic material
parameters. The quad only supplies UVs; its geometry does not approximate the anomaly. Step count is
clamped to 8..96, and the HLSL loop has a hard 96-step ceiling plus maximum-distance and surface-hit
breaks. Exposed controls cover quality, scale, deformation, speed, noise frequency, colours, and
emissive strength.

Open the showcase and select **RAYMARCHED ANOMALY**. Increasing steps, maximum distance, deformation,
or noise frequency raises pixel cost. The internal ray camera is local to the panel, the effect does
not write analytical scene depth, and its glow is an approximation rather than a separate blur pass.
