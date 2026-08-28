# Construction / Spawn

## What it demonstrates

This experiment progressively materializes a cube with a noisy clipping boundary, procedural
diagnostic grid, and bright assembly edge. It demonstrates a parameter-driven masked reveal rather
than fading the whole object transparently.

## Rendering pipeline

`ConstructionSpawn.ush` receives object-local position from the material graph. The engine cube's
known `-50..50` local bounds are normalized, then `Progress` is compared with height plus a small
three-dimensional noise offset. The result drives the masked material's opacity mask. The same
signed distance produces the emissive construction boundary, while local coordinates generate the
grid and animated packets.

`AConstructionSpawnExperimentActor` owns a dynamic material instance. Editor settings and callable
functions update `Progress`, time, grid/noise scales, edge width, colours, and emission. No global
state or gameplay system is involved.

## Editor controls and showcase

Open `/SandboxShaders/Showcase/SandboxShaders_Showcase`, select `Construction / Spawn`, and use
`reset_effect`, `complete_effect`, or `restart_animation`. Disable `auto_animate` to scrub `Progress`
manually from zero to one. A gameplay spawn system could later write that same scalar.

## Performance and limitations

The shader performs a small fixed amount of noise work and uses masked rendering. It assumes the
engine cube's local bounds; supporting arbitrary meshes would require passing bounds explicitly or
using normalized mesh UV/data, which is intentionally deferred.
