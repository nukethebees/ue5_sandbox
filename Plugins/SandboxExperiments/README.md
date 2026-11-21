# Open a showcase

Enable `SandboxExperiments` in `Sandbox.uproject`, rebuild with `cmake --build --preset debug-game`, and open the editor.

**Shaders.** Choose **Window → Sandbox Shaders → Open Shader Showcase**. Select an actor to edit its settings. [Effects catalog](../SandboxShaders/README.md#shader-experiments)

**UI.** In `/SandboxExperiments/UIExamples`, right-click a widget and choose **Run Editor Utility Widget**. For a command-line benchmark, run `cmake --build --preset debug-game --target <target>`.

| Widget | Shows | Benchmark target |
| --- | --- | --- |
| `EUW_SandboxUIShowcase` | Live SandboxUI gallery | — |
| `EUW_HeatmapRDGShowcase` | RDG versus Slate custom vertices | `heatmap-benchmark` |
| `EUW_Radar3DShowcase` | Production radar with synthetic contacts | `radar-3d-benchmark` |
| `EUW_Scatter3DShowcase` | Depth-tested point clusters | `scatter-3d-benchmark` |
| `EUW_VolumeHeatmap3DShowcase` | Dense volume rendered in slices | `volume-heatmap-3d-benchmark` |
