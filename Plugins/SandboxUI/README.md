# SandboxUI radar

Share an `FRadarFrameStore` with `SRadarWidget`. Fill `next()`, call `publish()`, then call `render()` from the UI update. The frame supplies contact positions and glyphs. `FRadarStyle` controls their appearance.

In a level, edit **Sandbox → Presentation → Radar** on `ATestBatchOrchestrator` for range, grid density, and glyph scale. [Preview synthetic contacts](../SandboxExperiments/README.md)
