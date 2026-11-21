# Warp / Distortion Field

Refract a checker panel through a translucent sphere. Select **WARP FIELD** in the
[shader showcase](../../README.md). Pause or restart the animation in Details. Tune distortion,
noise scale and speed, pulse frequency, opacity, colours, and edge intensity on
`AWarpFieldExperimentActor`.

`M_WarpField` calls `Shaders/Private/WarpField/WarpField.ush`. Two projections of animated noise
vary the refractive index. A Fresnel term lights the edge. The result depends on the project’s
translucency and refraction settings.
