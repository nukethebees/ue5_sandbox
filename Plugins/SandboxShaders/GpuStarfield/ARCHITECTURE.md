# GPU Starfield architecture

The actor generates immutable 32-byte star records, then transfers them to a render-thread-owned
structured buffer in its scene proxy. One indexed, instanced quad mesh batch per visible view reads
the buffer with stereo-correct instance IDs. The vertex factory performs camera-relative LWC math,
billboard expansion, and subpixel energy correction; the additive material supplies circular falloff,
brightness, temperature variation, and optional bright-star shaping.

Near, middle, and distant stars share one interleaved buffer and draw. Optional galactic haze reuses
the proxy and quad for one additional bufferless background draw. Opaque scene depth occludes the
background, which does not write gameplay depth. There is no tick or per-star CPU work after
generation.

The always-visible primitive path intentionally trades primitive-level frustum, distance, and
occlusion culling for stable shader-relative positions after long camera travel. Per-star culling is
not implemented; GPU work therefore scales with unculled quads and covered pixels. Measurements are
target-hardware decisions, not guarantees from historical showcase captures.

See [README.md](README.md) for use and benchmarks.
