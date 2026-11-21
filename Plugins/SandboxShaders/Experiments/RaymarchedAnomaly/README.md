# Raymarched Energy Anomaly

Raymarch a noisy sphere and rotating torus on a flat quad. Select **RAYMARCHED ANOMALY** in the
[shader showcase](../../README.md). Tune scale, deformation, speed, noise frequency, colours, and
emission on `ARaymarchedAnomalyExperimentActor`.

Set 8–96 steps. `Shaders/Private/RaymarchedAnomaly/RaymarchedAnomaly.ush` marches a panel-local ray
until a surface hit or the distance limit. It estimates normals by finite differences and
accumulates glow along the ray. More steps and longer rays cost GPU time. The effect leaves scene
depth unchanged.
