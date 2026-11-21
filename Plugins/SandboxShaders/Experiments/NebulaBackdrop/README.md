# Procedural Nebula Backdrop

Place a distant nebula on a translucent card. Select **NEBULA BACKDROP** in the
[shader showcase](../../README.md). Tune the palette, density, brightness, texture placement,
parallax, edge fade, and drift on `ANebulaBackdropExperimentActor`.

Four offset density samples give the card parallax. One flow sample animates it. The card takes one
translucent draw, tests scene depth, and leaves depth unchanged. Use it for distant scenery. The
plugin ships the deterministic `nebula_soft` and `nebula_flow` textures from Image Lab.
