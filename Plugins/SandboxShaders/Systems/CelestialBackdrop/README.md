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

The body uses up to three concentric engine spheres:

- An opaque unlit surface writes depth and supplies the silhouette, procedural palette, terminator,
  and optional night-side emission.
- An optional translucent cloud shell adds independently rotating procedural coverage.
- An optional additive atmosphere shell adds a controllable analytic limb and forward highlight.

All patterns use three-dimensional sphere directions, so there is no longitude seam or polar UV
pinching. The shaders have fixed bounded work, do not raymarch, and do not sample scene textures.
The normal maximum cost is three draws per body; disabling clouds or atmosphere removes that draw.

A physically based atmosphere, a raymarched body, and a camera-facing impostor were deliberately
not used. They respectively add cost and tuning complexity, add unnecessary volume work, or break
under the unusually close camera positions this game can produce.

## Usage

1. Place **Celestial Backdrop Actor** in a level.
2. Position it as scenery and set `Body Radius` in centimetres. Uniform actor scaling is supported,
   although using the radius property keeps authored intent clearer.
3. Set `Sun Direction` to the world-space direction from the body towards its light source.
4. Choose `Rocky` or `Gas Giant`, tune the grouped settings, or press one of the four preset buttons.
5. Clouds rotate around world Z; a negative speed reverses them and zero pauses them.

Changes made in the Details panel update through the construction path. Runtime code may change the
public `settings` struct and call `apply_settings()`. Appearance presets intentionally preserve the
actor transform, body radius, sun direction, and close-approach settings.

## Important controls

- **Surface:** two day colours, night tint and brightness, detail scale/strength, breakup,
  quantisation steps, seed, gas-band count/strength/warp, and patterned emission.
- **Lighting:** world-space sun direction and terminator softness.
- **Clouds:** enablement, day/night colours, altitude, coverage, opacity, breakup, phase, and signed
  degrees-per-second rotation speed.
- **Atmosphere:** enablement, colour, shell thickness, density, limb intensity, and limb falloff.
- **Close Approach:** enablement and the minimum camera-distance ratio.

The supplied examples are Earth-like rocky, orange Hive world, dark emissive alien world, and gas
giant. They are code presets rather than separate material instances, so every resulting value
remains directly editable.

## Close approach

With the close guard enabled, the materials begin shrinking every shell around the fixed actor
origin when the camera comes inside `outer radius * minimum camera distance ratio`. The angular size
then stops increasing, preventing entry through the atmosphere or surface without moving the actor,
ticking, or consulting a player camera manager. At the exact body centre the geometry collapses to
nothing. This is an intentional background-scenery cheat rather than physical traversal.

## Limitations

- The surface is procedural-only: there are no authored albedo maps, terrain displacement, or
  close-up ground detail.
- Rings, eclipses, cloud shadows, and physical multiple scattering are outside the first version.
- Several overlapping translucent celestial bodies may need ordinary Unreal translucency sort
  priority adjustments.
- The engine sphere is appropriate while the close guard caps screen coverage; this is not a
  walkable or orbit-to-ground planet renderer.
