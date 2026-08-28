# Lesson 02 — Shader Coordinates and Analytic Shapes

## Goal

Build recognizable 2D visuals from interpolated coordinates and distance tests rather than CPU
shape geometry.

## What you should understand after this lesson

- UVs arrive normalized because the quad vertices carry them and rasterization interpolates them.
- Remapping `[0,1]` to `[-1,1]` puts the origin at the preview centre.
- A distance field turns a geometric boundary into a scalar that `step` or `smoothstep` can shade.
- Aspect correction is part of the coordinate metric, not a special circle operation.
- `frac` creates repetition cheaply, but high-frequency repetition can alias.

## Architecture

Five small UI materials each contain one Custom expression. Every expression includes
`Shaders/Lesson02/Lesson02.ush` and calls one focused function. Each preview remains a single Slate
quad; the separate materials keep the examples inspectable without a mode-switching framework.

## CPU-side code

The showcase loads the materials and assigns brushes to fixed-size `SImage` widgets. The 520 by 260
aspect preview supplies an `AspectRatio` scalar with a default of `2.0`. No circle, box, line, or grid
vertex arrays are created.

## GPU-side code

The core operations are visible directly in `Lesson02.ush`:

```hlsl
float2 centered = uv * 2.0 - 1.0;
float circle_distance = length(centered);
float box_distance = max(abs(centered.x) - half_width,
                         abs(centered.y) - half_height);
float repeated = frac(uv.x * cell_count);
```

`step` makes a hard classification. `smoothstep` maps a small interval continuously, which produces
an antialiased-looking transition when its width is based on `fwidth`. `lerp` interpolates colours
using those masks.

## Data flow

The CPU submits the same four logical quad corners for every card. The vertex shader transforms
them; rasterization produces fragments and interpolated UVs; the pixel shader evaluates the
analytic function independently for each covered pixel. Only the resulting colour is written to
the active Slate render target.

## Unreal-specific machinery

`UMaterialExpressionCustom::IncludeFilePaths` causes Unreal's material translator to place the
external include before the generated Custom function. The node code then calls the included
function. Unreal still generates the surrounding material shader and selects the platform
permutation; this is not an `FGlobalShader`.

The shader executes in the pixel stage because the Custom output feeds the UI material's final
colour. Derivatives such as `fwidth` are therefore available over neighboring fragment lanes.

## Performance notes

Analytic shapes trade CPU geometry, uploads, and draw bookkeeping for pixel ALU. This is attractive
when many shapes can share a draw or when the visual changes without topology changes. It can be a
bad trade when a shader covers a large mostly-empty rectangle, uses expensive math per pixel, or
creates enough distinct materials to defeat batching.

`smoothstep` does not solve temporal aliasing or arbitrarily dense grids. Zoomed-out procedural
detail still needs filtering or a level-of-detail policy.

## Things deliberately not abstracted yet

The lesson functions remain explicit rather than sharing a general SDF library. Each card has its
own material. There is no shape buffer, instance list, atlas protocol, or GPU dispatch.

## Exercises

1. Add another concentric ring without adding geometry.
2. Change ring thickness and preserve antialiasing.
3. Combine the axis distances into a crosshair with a center gap.
4. Build a radar grid with major and minor divisions.
5. Add a diagonal line segment using distance to a finite segment.
6. Make the box corners rounded by modifying its signed distance.
