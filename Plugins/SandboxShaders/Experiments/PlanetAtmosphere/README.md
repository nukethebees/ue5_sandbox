# Planet Atmosphere

## What it demonstrates

This experiment uses two cheap spheres to create a readable planet and atmospheric limb. The inner
sphere shows a sun-driven day/night terminator. A slightly larger translucent shell approximates
the longer optical path seen at grazing view angles, producing the bright atmospheric rim.

## Rendering pipeline

`PlanetAtmosphere.ush` contains the surface-lighting and atmosphere functions. Custom material
expressions pass world-space vertex normals, the camera vector, and scalar/vector parameters into
those functions. `APlanetAtmosphereExperimentActor` creates dynamic instances for both materials and
supplies the normalized sun direction, colours, density, limb exponent, scale-height approximation,
and emission. The opaque inner sphere and additive outer shell then composite conventionally.

The calculation is intentionally not physical scattering. Optical depth is one bounded exponential
using the view/normal cosine; it is suitable for a gallery experiment but not a replacement for
Unreal's atmosphere systems.

## Editor controls and showcase

Open `/SandboxShaders/Showcase/SandboxShaders_Showcase` and select `Planet Atmosphere`. Change the
settings directly, or use `reset_sun_direction`. Density and limb power make the approximation easy
to exaggerate. A later gameplay integration could feed the system star direction into the same
`SunDirection` material parameter.

## Limitations

The shell assumes spherical supporting geometry and ignores multiple scattering, shadows through
clouds, and true scale-height integration. Translucent overdraw is limited to one low-cost shell.
