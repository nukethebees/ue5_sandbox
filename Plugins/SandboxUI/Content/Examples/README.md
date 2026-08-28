# SandboxUI showcase

`WBP_SandboxUIShowcase` is an editor-only visual catalogue of the reusable Slate widgets supplied
by SandboxUI. Its live example data and generated layout are implemented by
`USandboxUIShowcase` in the `SandboxUIExamples` editor module.

For an interactive view, right-click `EUW_SandboxUIShowcase` and choose **Run Editor Utility
Widget**. The utility embeds `WBP_SandboxUIShowcase`, so both entry points display the same gallery
and example data. Use the WBP for normal UMG inspection and the EUW for an interactive, scrollable
editor tab.

When a reusable SandboxUI widget is added, a representative live example should normally be added
to this gallery as well. Keep example-only data and presentation in `SandboxUIExamples`; production
widgets must not depend on the showcase.

`EUW_HeatmapRDGShowcase` is a separate editor utility for the experimental RDG/GPU heatmap path.
Its UI is implemented by `UHeatmapRDGShowcase` in the editor-only `Experiments` module. Right-click
the asset and choose **Run Editor Utility Widget** to compare deterministic patterns and grid sizes.

`EUW_Radar3DShowcase` demonstrates the experimental CPU-contact-to-RDG-to-Slate pipeline without a
level, actors, components, or scene capture. Right-click it and choose **Run Editor Utility Widget**
to see the fixed-camera radar box and its animated synthetic contact.

`EUW_Scatter3DShowcase` demonstrates a dirty-driven RDG 3D scatter plot with deterministic point
clusters, a fixed camera, depth-tested instanced markers, and scaling controls through 65,536
points. Right-click it and choose **Run Editor Utility Widget** to launch the standalone example.

`EUW_VolumeHeatmap3DShowcase` demonstrates a dense 3D scalar field rendered as view-aligned,
translucent RDG slices. It includes deterministic cloud and shell patterns, grid and slice scaling,
camera controls, and a benchmark that separates API submission from GPU upload/raster time.
