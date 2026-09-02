# Bounded Nebula Volume

This experiment raymarches a procedural dust cloud inside one artist-sized cube. The material
finds the camera ray's entry and exit in local box space, works from outside or inside the cube, and
clips the march against opaque scene depth. Front-to-back accumulation exits once opacity reaches
95%, and a soft boundary makes adjacent transparent space visually clean.

`Extent` controls the volume's dimensions in world units; leave the actor's Transform Scale at one.
`Feature Size` and `Detail Size` independently control the physical size of cloud structures, so
making the volume larger reveals more formations instead of stretching one formation. `Seed`
selects a deterministic variation for each nebula instance.

Density combines infinite, seeded 3D value noise with two skewed projections of the deterministic
`nebula_soft` texture. The procedural macro field breaks up texture repetition while the texture
provides inexpensive detail. One `nebula_flow` lookup per pixel offsets both projections, giving
the volume slow coherent motion. The promoted textures live in this plugin, so there is no runtime
dependency on the Sandbox Image Lab.

## Quality

- **Low:** 12 steps, suitable for larger screen coverage.
- **Balanced:** 24 steps and the default showcase setting.
- **High:** 40 steps for close inspection.

The shader has a hard 48-step ceiling. Cost is one translucent draw, approximately two density
samples and one procedural 3D noise evaluation per executed step, plus one flow sample. Screen
coverage, overlap, resolution, and early opacity exit determine the actual GPU cost; avoid stacking
several full-screen volumes. Stable per-pixel jitter hides most low-step banding, with temporal
anti-aliasing providing the smoothest result.

The translucent material does not write scene depth and has the usual translucent sorting limits.
Open `/SandboxShaders/Showcase/SandboxShaders_Showcase` and select **NEBULA VOLUME** to edit it.
