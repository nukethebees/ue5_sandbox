# Bounded Nebula Volume

Fly through a dust cloud inside a cube. Select **NEBULA VOLUME** in the
[shader showcase](../../README.md). Set `Extent` in world units and leave Transform Scale at 1. Use
`Feature Size` and `Detail Size` to size formations independently of the volume. Set `Seed` for
repeatable variation.

## Quality

| Preset | Density cache per actor (R8) | March steps | Use |
| --- | --- | --- | --- |
| Low | 128³ · 2 MiB | 24 | Large screen coverage |
| Balanced (default) | 256³ · 16 MiB | 48 | General use |
| High | 256³ · 16 MiB | 96 | Close fly-throughs |

The compute shader rebuilds density when extent, feature size, detail size, seed, or
`Volume Resolution` changes. Use **Regenerate Density** to force a refresh.

Each translucent draw samples `nebula_flow` once per pixel and density once per step. The march
clips against opaque depth and exits at 95% opacity or the box boundary. It leaves scene depth
unchanged. Limit overlapping full-screen volumes to control cost and sorting artifacts.
