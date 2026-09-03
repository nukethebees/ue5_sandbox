# Bounded Nebula Volume

This experiment raymarches a procedural dust cloud inside one artist-sized cube. The material
finds the camera ray's entry and exit in local box space, works from outside or inside the cube, and
clips the march against opaque scene depth. Front-to-back accumulation exits once opacity reaches
95%, and a soft boundary makes adjacent transparent space visually clean.

`Extent` controls the volume's dimensions in world units; leave the actor's Transform Scale at one.
`Feature Size` and `Detail Size` independently control the physical size of cloud structures, so
making the volume larger generates more formations instead of stretching one formation. `Seed`
selects a deterministic variation for each nebula instance. `Volume Resolution` controls the cubic
density cache resolution and defaults to 128.

When structural settings change, a compute shader generates domain-warped, periodic 3D density
directly into a transient single-channel R8 volume texture. The raymarch samples this true spatial
density once per step, avoiding the sheet-like layering caused by projected 2D textures. One
`nebula_flow` lookup per pixel offsets the volume coordinates for slow coherent motion. The flow
texture lives in this plugin, so there is no runtime dependency on the Sandbox Image Lab.

A 128-cubed R8 density volume uses approximately 2 MiB of VRAM per actor. Generation happens once
after creation and again only when Extent, Feature Size, Detail Size, Seed, or Volume Resolution
changes. The `Regenerate Density` editor action forces a refresh without changing settings.

## Quality

- **Low:** 12 steps, suitable for larger screen coverage.
- **Balanced:** 24 steps and the default showcase setting.
- **High:** 40 steps for close inspection.

The shader has a hard 48-step ceiling. Recurring cost is one translucent draw, one volume-density
sample per executed step, and one flow sample per pixel. Screen coverage, overlap, resolution, and
early opacity exit determine the actual GPU cost; avoid stacking several full-screen volumes.
Stable per-pixel jitter hides most low-step banding, with temporal anti-aliasing providing the
smoothest result.

The translucent material does not write scene depth and has the usual translucent sorting limits.
Open `/SandboxShaders/Showcase/SandboxShaders_Showcase` and select **NEBULA VOLUME** to edit it.
