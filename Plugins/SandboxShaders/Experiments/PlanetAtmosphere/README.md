# Planet Atmosphere

Light a planet with an opaque day/night surface and an additive atmospheric shell. Select
**Planet Atmosphere** in the [shader showcase](../../README.md). Adjust sun direction, colours,
density, limb power, scale height, or emission on `APlanetAtmosphereExperimentActor`. Use
`reset_sun_direction` to restore the light direction.

`PlanetAtmosphere.ush` approximates the bright limb with one bounded exponential of the angle
between the view and surface normal. Use spherical geometry. The approximation omits multiple
scattering and cloud shadows.
