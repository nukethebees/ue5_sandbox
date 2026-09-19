# SandboxISMC architecture

The custom component owns instance transforms in CPU-side arrays, prepares changes on the game
thread, and commits a render-thread upload for its scene proxy. The paired comparison keeps mesh,
material, transforms, mobility, visibility, and movement equivalent while retaining the engine
ISMC's normal GPU Scene and culling path as part of that renderer's cost.

The experiment separates caller preparation, component API work, instance creation, render-thread
upload, and frame/GPU timing. Render-thread counters measure upload cost without synchronous
readback; CSV results and optional Insights traces are diagnostic evidence, not a universal renderer
ranking. The custom path commits every tick and is intentionally a focused update-cost prototype,
not a replacement for every engine ISMC feature.

See [README.md](README.md) for the experiment entry point.
