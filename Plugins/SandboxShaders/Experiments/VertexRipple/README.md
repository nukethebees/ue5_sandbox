# Vertex Ripple / World Position Offset

Displace a 64×64-subdivision grid with a radial shockwave. Adjust origin, amplitude, wavelength,
speed, width, falloff, and colour on the actor. Keep amplitude within the mesh bounds to avoid
clipping and faceting.

`M_VertexRipple` calls `Shaders/Private/VertexRipple/VertexRipple.ush` in both shader stages. The
same travelling ring drives vertex displacement and the emissive crest. Material Time animates it
without an actor tick. Collision keeps its original shape.
