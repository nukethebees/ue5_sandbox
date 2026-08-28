# Warp / Distortion Field

This experiment demonstrates Unreal's translucent refraction path on a simple sphere. The material
combines two projections of procedural FBM, an explicit animation clock, a breathing pulse, and a
Fresnel boundary. Noise modulates the material's index-of-refraction input while the Fresnel term
provides the bright anomaly edge.

`AWarpFieldExperimentActor` creates a dynamic instance of `M_WarpField` and forwards distortion
strength, noise scale/speed, pulse frequency, opacity, colours, and edge intensity. Its Custom
expressions include `Shaders/Private/WarpField/WarpField.ush`; one returns emissive/opacity and the
other returns the refractive index. The checker panel behind the sphere exists solely to make the
background warp easy to read.

Open the showcase and select **WARP FIELD**. The animation can be paused or restarted in the Details
panel. This is an artistic surface refraction effect, not gravitational lensing or a physically
correct volume. Results remain sensitive to Unreal's translucency/refraction project settings.
