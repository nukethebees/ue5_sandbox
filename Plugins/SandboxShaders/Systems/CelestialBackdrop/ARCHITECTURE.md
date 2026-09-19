# Celestial Backdrop architecture

Layered mode composes an opaque sphere with optional cloud, atmosphere, and ring layers. Analytic
mode uses one translucent sphere proxy whose pixel shader intersects the body, shells, and ring
plane before compositing them front-to-back. Both use bounded procedural evaluation with no
raymarching or scene-texture sampling; Layered favors ordinary opaque depth while Analytic favors a
single draw at the cost of translucent depth behaviour.

Profiles own reusable appearance only. Actor transform, body radius, sun direction, and
close-approach policy stay per actor, so one profile can serve bodies at different scales and
locations. The generated analytic material remains sourced from its Lispb graph and is refreshed by
its CMake target.

The close guard shrinks represented geometry around the fixed actor origin when a camera is too
near, capping apparent size without introducing gameplay traversal. This deliberate scenery cheat,
the procedural-only surface, and ordinary translucency sorting limits make the system unsuitable for
walkable terrain or physically detailed orbital rendering.

See [README.md](README.md) for placement and controls.
