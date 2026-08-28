# Vertex Ripple / World Position Offset

This experiment demonstrates vertex-stage displacement rather than a pixel-only effect. A
plugin-owned 64x64-subdivision grid supplies enough geometry for a radial shockwave to visibly lift
and bend the surface. The material uses the same travelling-ring function for World Position Offset
and emissive crest shading, making the colour track the displaced geometry.

`M_VertexRipple` includes `Shaders/Private/VertexRipple/VertexRipple.ush` from two Custom
expressions: one compiled for the vertex path and one for the pixel path. The actor forwards origin,
amplitude, wavelength, speed, width, falloff, and colour settings through a dynamic material
instance. Animation comes from Unreal's material Time input, so no actor tick is needed.

The grid has fixed density and bounds. Very large amplitudes can exceed its authored bounds or show
faceting; there is no adaptive tessellation, collision deformation, or Nanite-specific path.
