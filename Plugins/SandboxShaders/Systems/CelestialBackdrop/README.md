# Celestial Backdrop

`ACelestialBackdropActor` renders a configurable planet, moon, or stylised large body intended only
as background scenery. It has no collision, overlap, physics, navigation, shadow, or gameplay
responsibilities. Flying a ship through its authored location has no simulation effect.

## Design

The Planet Atmosphere experiment established that an opaque sphere and one additive shell can make
a cheap readable limb. The GPU Starfield supplied the useful pattern of an editor-facing settings
struct feeding a rendering-only component, while Nebula Backdrop and Nebula Volume demonstrated
bounded procedural detail and explicit quality costs. The production system keeps those ideas but
does not share the prototype actor, shader, materials, or parameter contract.

The actor offers two rendering modes with the same settings and profiles:

- **Layered** uses up to three concentric engine spheres and one optional plane. It is the simpler
  reference path and retains opaque surface depth.
- **Analytic** uses one translucent sphere proxy and one AlphaComposite material. The pixel shader
  intersects the body, cloud shell, atmosphere shell, and ring plane analytically, then composites
  them front-to-back. Disabled features remain branches in that shader, but the body stays one draw.

The layered mode is assembled from:

- An opaque unlit surface writes depth and supplies the silhouette, procedural palette, terminator,
  and optional night-side emission.
- An optional translucent cloud shell adds independently rotating procedural coverage.
- An optional additive atmosphere shell adds a controllable analytic limb and forward highlight.
- An optional two-sided masked ring plane adds procedural radial bands and breakup. The opaque
  surface depth naturally separates the near and far halves.

All patterns use three-dimensional sphere directions, so there is no longitude seam or polar UV
pinching. The shaders have fixed bounded work, do not raymarch, and do not sample scene textures.
The layered mode costs at most four draws per body; disabling clouds, atmosphere, or rings removes
the corresponding draw. Analytic mode always costs one draw and avoids self-sorting between those
layers, at the cost of more pixel shader work and translucent scene-depth behaviour. Polar caps,
storms, ring shadows, and emission patterns are folded into the surface evaluation in both modes.

A physically based atmosphere, a raymarched body, and a camera-facing impostor were deliberately
not used. They respectively add cost and tuning complexity, add unnecessary volume work, or break
under the unusually close camera positions this game can produce.

## Usage

1. Place **Celestial Backdrop Actor** in a level.
2. Position it as scenery and set `Body Radius` in centimetres. Uniform actor scaling is supported,
   although using the radius property keeps authored intent clearer.
3. Set `Sun Direction` to the world-space direction from the body towards its light source.
4. Select `Render Mode`. Use **Analytic** for the one-draw prototype or **Layered** when opaque
   surface depth or conventional material inspection is more important.
5. Assign a **Celestial Backdrop Profile** for a reusable look, tune local settings, or press one of
   the four preset buttons.
6. Enable rings when needed and set their tilt independently. Clouds rotate around world Z; a
   negative speed reverses them and zero pauses them.

Author apparent size by the ratio between radius and camera distance, not by either number alone:

```text
angular diameter = 2 * atan(body radius / camera distance)
```

Useful starting points are 5 degrees for a distant secondary body, 15 degrees for a prominent moon,
and 30--40 degrees for a hero planet that dominates the sky. For example, a 100 km radius at 322 km
from the combat origin occupies about 34.5 degrees. Increasing radius and distance by the same factor
preserves its screen size while making the authored scale relationship larger.

Changes made in the Details panel update through the construction path. Runtime code may change the
public `settings` struct and call `apply_settings()`. Profiles contain appearance only: body radius,
sun direction, transform, and close-approach policy always remain per actor. Enable `Override Profile
Appearance` to use that actor's local appearance fields without breaking its profile reference.
Pressing a preset button enables this override automatically.

Create a profile through **Content Browser > Add > Miscellaneous > Data Asset**, selecting
`CelestialBackdropProfile`, then assign it to any number of backdrop actors. Ten editable example
profiles live under `CelestialBackdrop/Profiles`:

- Earth-like, ocean, ice, and toxic terrestrial worlds.
- Barren and molten airless worlds.
- Hive and dark-alien emissive worlds.
- Gas giant and ringed ice giant worlds.

Editing a profile refreshes actors that reference it in open editor worlds. The profile controls
appearance only, so the same preset can be reused at different sizes, positions, and sun directions.

The analytic material is generated from
`Source/SandboxCelestials/Private/materials/CelestialAnalytic.lispb`. After changing its graph,
regenerate the asset with:

```text
cmake --build --preset debug-game --target generate-celestial-analytic-material
```

## Important controls

- **Surface:** two day colours, night tint and brightness, detail scale/strength, breakup,
  quantisation steps, seed, gas-band count/strength/warp, and noise, Hive Cells, or Molten Cracks
  emission patterns.
- **Accents:** optional polar caps and a controllable elliptical gas-giant storm.
- **Lighting:** world-space sun direction and terminator softness.
- **Clouds:** enablement, day/night colours, altitude, coverage, opacity, breakup, phase, and signed
  degrees-per-second rotation speed.
- **Atmosphere:** enablement, colour, shell thickness, density, limb intensity, and limb falloff.
- **Rings:** inner/outer radius, tilt, two-colour gradient, derivative-filtered banding, breakup,
  edge softness, emission, and a cheap aligned surface-shadow approximation.
- **Close Approach:** enablement and the minimum camera-distance ratio.

The shader showcase uses all ten supplied profiles. Its original four-body row keeps Earth-like and
gas giant in Layered mode and Hive world and dark alien in Analytic mode for direct A/B inspection.
A second six-body row shows the extended profiles in Analytic mode. The real `GameRuntime` map
uses the analytic path for its distant Hive homeworld and contains an Earth-like hero scale
reference: 200 km in diameter, centred about 322 km from the world origin. It occupies approximately
34.5 degrees from the origin, making its scale unmistakable beside ships. The four bodies in the
shader showcase are intentionally much smaller from its overview camera so they fit side by side for
profile comparison. Profiles remain independent of render mode, body size, and lighting direction.

World-space scale alone does not make rendering more expensive: the proxy retains the same vertex
count and draw count. Cost primarily follows covered screen pixels and overdraw. A 35-degree analytic
body therefore costs more than a 5-degree body because it shades more pixels, while moving a planet
farther away and increasing its radius proportionally has essentially the same rendering cost.
Layered bodies add separate surface, cloud, atmosphere, and ring draws; use Analytic mode for large
hero bodies unless opaque depth behaviour is required.

## Close approach

With the close guard enabled, the active geometry and its represented layers shrink around the fixed
actor origin when the camera comes inside `outer radius * minimum camera distance ratio`.
The guard uses the ring radius when rings are enabled. Angular size then stops increasing,
preventing entry through the atmosphere, rings, or surface without moving the actor, ticking, or
consulting a player camera manager. At the exact body centre the geometry collapses to nothing.
This is an intentional background-scenery cheat rather than physical traversal.

## Limitations

- The surface is procedural-only: there are no authored albedo maps, terrain displacement, or
  close-up ground detail.
- Physical ring shadows, eclipses, cloud shadows, and multiple scattering are outside the system.
- Ring edges use masked coverage for predictable cost and sorting, so they are intentionally more
  graphic than dusty translucent rings.
- Several overlapping translucent celestial bodies may need ordinary Unreal translucency sort
  priority adjustments.
- Analytic mode does not write the physical body's depth: its single translucent proxy is sorted as
  one object. This is appropriate for isolated background scenery, but Layered mode is safer when
  opaque or translucent geometry must closely intersect a planet.
- Analytic mode uses bounded sphere and plane intersections rather than raymarching. It cannot
  represent terrain relief, volumetric clouds, or thick dusty rings without adding another model.
- The engine sphere is appropriate while the close guard caps screen coverage; this is not a
  walkable or orbit-to-ground planet renderer.
