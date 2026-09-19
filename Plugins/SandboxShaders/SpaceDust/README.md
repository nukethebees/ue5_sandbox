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

The shader uses the actual world velocity to derive screen-plane streak
direction and a bounded pixel length.  Camera rotation changes the observed
field naturally, but zero translation produces no velocity-driven brightness
or stretch.  The material is additive/unlit, tests normal scene depth, and
does not write depth; opaque hulls can occlude dust.

## Tuning

`FPlayerShipConfig::space_dust` exposes the initial controls:

- enable flag, count, seed, and volume dimensions;
- size, brightness, and colour;
- minimum/full visible speed;
- streak time and maximum pixel length;
- volume-edge fade.

The default is 1,024 instances.  It is intended to be subtle at low speed;
raise count or brightness only after checking the motion cases below.  Count
and enabled state recreate the proxy.  Other settings and motion are tiny
per-frame parameter updates.

## Measurement

`SpaceDustSubmit` is a CSV render-thread timing scope around dynamic mesh
submission.  Unreal Insights/CSV GPU and draw statistics expose the single
draw and its instance count.  Compare disabled, the default 1,024 instances,
and a deliberately excessive count such as 4,096.  Submission should remain
effectively constant; GPU time should scale approximately with instance count
and covered pixels.

## Validation checklist

1. Stopped ship: dust is near-invisible and has no translational streaking.
2. Forward acceleration: dust flow and visibility increase with speed.
3. Sideways drift: flow is lateral in screen space.
4. Rotate while drifting: flow continues to describe the original world
   velocity rather than the new ship forward direction.
5. Rotate in place: the field responds to view rotation without artificial
   translation.
6. Maximum speed: streaks remain short and bounded, without visible wrapping.
7. Nearby opaque geometry: dust does not visibly render through hulls.
8. Long travel, teleports, and reset: no pattern collapse, jitter, or
   accumulated precision loss.

`Source/SandboxTests/unit/test_space_dust.cpp` covers settings normalisation
and large/negative translation-phase wrapping.  The material generator test
also covers the vertex-colour material input used by this renderer.
