# Procedural Energy Shield

This experiment turns an ordinary sphere into an exaggerated science-fiction force field. It
demonstrates how a small Unreal material graph can act as the connection layer around plugin-owned
HLSL instead of reproducing procedural logic as dozens of material nodes.

## Pipeline

The material asset is
`/SandboxShaders/Experiments/EnergyShield/M_EnergyShield`. Its Custom expression includes
`/Plugin/SandboxShaders/Private/EnergyShield/EnergyShield.ush`; that file also uses the small noise
helpers in `Shaders/Common/ProceduralNoise.ush`.

Standard material expressions provide world/object position, pixel normal, camera vector, game
time, and named scalar/vector parameters to `sbx_energy_shield`. The HLSL builds a triplanar hex
grid, animated noise distortion, a scanning band, and a Fresnel edge, then returns emissive colour
and opacity. The material routes RGB to Emissive Color and alpha to Opacity.

`AEnergyShieldExperimentActor` owns the sphere and creates a dynamic material instance. Its
`FEnergyShieldSettings` properties update the named material parameters during construction, so
editing the actor does not require shader-source changes.

## Viewing it

Open `/SandboxShaders/Showcase/SandboxShaders_Showcase`, or choose **Window > Sandbox Shaders >
Open Shader Showcase**. The shield is the cyan sphere on the left. PIE uses the placed showcase
camera, while the effect also animates in the editor viewport.

## Limitations

- Distortion deforms procedural coordinates; it does not refract scene colour.
- The translucent sphere is intended as an experiment and has the normal translucent sorting and
  overdraw costs.
- The implementation assumes the UE 5.8 material Custom-expression pipeline and SM5 or newer.
