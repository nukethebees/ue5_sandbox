# Tactical Scan

Sweep a diagnostic grid across opaque geometry inside a box. Select **TACTICAL SCAN** in the
[shader showcase](../../README.md). Move or rotate `ATacticalScanExperimentActor` to aim the scan.
Call `restart_scan`, pause playback, or tune speed, width, intensity, falloff, colour, and grid
scale.

`M_TacticalScan` calls `Shaders/Private/TacticalScan/TacticalScan.ush`. The post-process shader
reads `PostProcessInput0`, scene depth, and reconstructed world position. It projects valid surface
pixels onto the scan axis and draws a band from `-ScanRange` to `+ScanRange`.
