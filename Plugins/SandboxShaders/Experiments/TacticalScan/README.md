# Tactical Scan

This experiment demonstrates a bounded post-process material that reveals a diagnostic grid as a
scan plane moves across scene geometry. Unlike the mesh-only experiments, it reads
`PostProcessInput0`, SceneDepth, and Unreal's reconstructed absolute world position.

`ATacticalScanExperimentActor` owns a box and a local `UPostProcessComponent`. It passes its world
origin and local X/Y/Z axes, an explicit animation clock, scan range, width, falloff, and colour to
`M_TacticalScan`. The Custom expression includes
`Shaders/Private/TacticalScan/TacticalScan.ush`. The shader projects each reconstructed world
position onto the actor's world-space scan direction. The sweep travels from `-ScanRange` to
`+ScanRange`; only pixels with valid scene depth receive the emissive band and world-aligned grid.

Move or rotate the actor to reposition the scanning coordinate frame. Gameplay could later call
`restart_scan`, change the actor transform, or drive the exposed material values from an ability or
sensor system. The showcase actor exposes pause, speed, width, intensity, falloff, colour, and grid
scale. The effect is deliberately local: geometry outside the box is not post-processed.

Open `/SandboxShaders/Showcase/SandboxShaders_Showcase` and select **TACTICAL SCAN**. This is an
opaque-screen composite rather than a volumetric light, and it does not use CustomDepth or identify
individual gameplay objects.
