# Procedural Nebula Backdrop

This experiment renders a distant nebula on one artist-placed translucent card. Four fixed,
unrolled layers sample the plugin-owned density texture with small view-dependent offsets, producing
depth during camera translation without a raymarch. A generated flow texture adds slow coherent
drift. Soft UV-edge fading keeps the card boundary out of the composition.

`ANebulaBackdropExperimentActor` exposes the restrained blue-violet palette, density, brightness,
texture placement, parallax, edge fade, and animation controls. The material uses ordinary scene
depth testing, does not write depth, and costs one translucent draw plus five texture samples per
covered pixel. It is intended for distant composition rather than flying through the card.

The source textures are deterministic promotions of the `nebula_soft` and `nebula_flow` presets
from the Sandbox Image Lab. The committed copies keep this plugin independent of `SandboxImages` at
runtime.

Open `/SandboxShaders/Showcase/SandboxShaders_Showcase` and select **NEBULA BACKDROP** to edit it.
