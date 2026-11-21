# Construction / Spawn

Reveal a cube from bottom to top. `ConstructionSpawn.ush` masks fragments by local height and noise,
then lights the exposed edge and grid. The shader assumes the engine cube’s local bounds of −50 to
50 on each axis.

Select **Construction / Spawn** in the [shader showcase](../../README.md). Disable `auto_animate` to
scrub `Progress` from 0 to 1. Call `reset_effect`, `complete_effect`, or `restart_animation` to
control playback.
