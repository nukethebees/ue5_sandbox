# Space Dust

`USpaceDustComponent` is a specialised near-field velocity visualiser.  It is
separate from `GpuStarfield`: the starfield is an astronomical background;
space dust is a small player/camera presentation effect.

## Ownership and motion

`ATestSpaceShip` owns the component and attaches it to its gameplay camera.
The component is `bOnlyOwnerSee`, hidden from scene captures, reflection
captures, and ray tracing.  The normal player pawn view therefore sees dust,
while playerless observer, battle-viewer, editor, cinematic, and capture views
do not implicitly acquire it.

`FPlayerPresentation` supplies `PlayerReadView::velocity` every presentation
tick.  This is the native simulation's authoritative world-space translational
velocity; it is not inferred from an Unreal transform or reconstructed from
the ship's forward vector.  A sideways slide remains sideways after the ship
rotates.

The component derives a bounded translation phase from the camera's LWC
`FVector` world location each update.  It never integrates a float travel
offset, so pause, teleport, reset, and arbitrarily long journeys all obtain a
fresh phase from the authoritative presentation transform.

## Renderer

The scene proxy submits one instanced indexed-quad mesh per visible owner view.
There is no particle buffer, spawn list, compute dispatch, lifetime state, or
CPU particle loop.  The vertex shader obtains each instance's stable seed from
`SV_InstanceID`, hashes it into a local position, subtracts the translation
phase, and wraps it inside the camera-relative volume.

For each particle, the vertex shader analytically projects its current
camera-relative position and the position from which it would have been seen
`streak_seconds` earlier. Their difference supplies the particle's apparent
screen-space direction and pixel displacement. Sideways travel therefore
produces broadly parallel flow, while forward travel produces perspective
motion that grows radially and is stronger for nearby particles.

Both total world speed and per-particle pixel displacement gate visibility. A
particle that does not move far enough on screen fades away instead of
remaining as a minimum-sized star-like point. Streak length is the measured
pixel displacement capped by `maximum_streak_pixels`; the soft procedural
material turns the stretched quad into a subtle ellipse without a texture.
Camera rotation is not treated as translational motion. The material remains
additive/unlit, tests normal scene depth, and does not write depth, so opaque
hulls can occlude dust.

## Tuning

`FPlayerShipConfig::space_dust` exposes the initial controls:

- enable flag, count, seed, and volume dimensions;
- size, brightness, and colour;
- minimum/full world-speed visibility;
- streak time, minimum/full apparent-motion pixels, and maximum pixel length;
- volume-edge fade.

The default is intentionally sparse at 96 candidate instances, with apparent
motion fading from invisible at 0.75 pixels to fully visible at 4 pixels. The
candidate count can exceed the number visible in a frame because particles
outside the view, behind the camera, near volume edges, or below the motion
threshold contribute nothing. Count and enabled state recreate the proxy.
Other settings and motion are tiny per-frame parameter updates.

## Measurement

`SpaceDustSubmit` is a CSV render-thread timing scope around dynamic mesh
submission.  Unreal Insights/CSV GPU and draw statistics expose the single
draw and its instance count. Compare disabled, the default 96 instances, and
a diagnostic count such as 384. Submission should remain
effectively constant; GPU time should scale approximately with instance count
and covered pixels.

## Validation checklist

1. Stopped: dust is near-invisible, with no field of static white points.
2. Very slow motion: only occasional subtle particles appear; the background
   does not become a second starfield.
3. Sideways drift: sparse short streaks move laterally without space rain.
4. Forward acceleration: perspective motion emerges radially, with stronger
   displacement away from the view centre and on nearby particles.
5. Forward high speed: the effect strengthens but remains sparse and capped.
6. Combined forward and sideways movement: directions vary naturally per
   particle rather than sharing one global streak vector.
7. Rotate while drifting: motion continues to describe world velocity rather
   than the ship's new forward direction.
8. Rotate in place: no artificial translational dust becomes visible.
9. Nearby opaque geometry: dust does not visibly render through hulls.
10. Long travel, teleports, and reset: no wrapping, precision, or pattern
    regression appears.

`native/core/tests/space_dust_math_tests.cpp` covers settings and motion-threshold
normalisation, sparse defaults, large/negative translation-phase wrapping, and
deterministic seed positions with GoogleTest. The material compilation test
also covers the vertex-colour material input and custom vertex factory used by
this renderer.

## Flight lab and tuning

Select **Development > Space Dust Flight Lab** from the normal level-select
flow. It uses the standard `GameRuntime` map, starts the player in empty space,
and places a friendly capital ship ahead as a depth-occlusion reference. Use
normal controls to check forward flight, sideways drift, rotate-while-drifting,
and high speed.

In non-shipping builds, the console command `space_dust.preset` changes the
live player presentation effect without touching simulation state:

- `space_dust.preset off`
- `space_dust.preset default`
- `space_dust.preset strong`

`strong` preserves the active level's volume and colour while using 192
candidates, a 0.5-to-3-pixel motion fade, higher brightness, and a longer
bounded streak interval. It remains a controlled diagnostic preset rather
than restoring a dense field.
