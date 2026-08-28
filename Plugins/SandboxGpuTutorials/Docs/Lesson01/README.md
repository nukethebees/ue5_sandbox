# Lesson 01 — GPU Rendering Mental Model

## Goal

Follow a small editor-thread UI description through Slate, the render thread, the RHI, shader
execution, rasterization, and the target that finally receives the pixels.

## What you should understand after this lesson

- Slate paint code records draw elements; it does not synchronously paint pixels.
- Both showcase panels use GPU shaders and a rasterized quad.
- A material brush changes the shader and bound material state, not the basic Slate submission.
- CPU UObjects and brushes are not GPU resources, although they lead Unreal to resources that are.

## Architecture

The left widget calls `FSlateDrawElement::MakeGradient` with two stops. The right widget supplies an
`FSlateMaterialBrush` whose UI-domain material calls `sandbox_gpu_lesson01_gradient` from
`Shaders/Lesson01/Lesson01.ush`.

| Object | Side and owner | Lifetime |
|---|---|---|
| Slate lesson widget | CPU; shared pointer owned by the tab | Until the tab closes |
| `FSlateMaterialBrush` | CPU descriptor owned by the lesson resource | Same as the lesson widget |
| `UMaterialInterface` | CPU UObject, strongly referenced by the lesson resource | Same as the lesson widget; its package owns serialized data |
| Material render proxy/resources | Render-thread-facing state managed for the material | Created and retired through Unreal's render-resource lifecycle |
| Slate vertex/index data | CPU submission copied into renderer-managed buffers | Batched for the current Slate frame |

## CPU-side code

`OnPaint` records a gradient draw element for the left panel. `SImage` records a box draw element
with a material brush for the right panel. The editor thread returns after describing work; it does
not wait for rasterization to finish.

## GPU-side code

Slate supplies quad vertices. Vertex processing transforms those positions into the active Slate
target. Rasterization generates covered fragments and interpolates UVs. The selected pixel shader
then computes either the built-in gradient or the material colour:

```hlsl
float3 color = lerp(dark_blue, bright_cyan, uv.x);
color *= lerp(0.72, 1.0, uv.y);
```

## Data flow

1. The editor thread traverses the widget tree and records draw elements.
2. Slate batches compatible elements and prepares transient vertex/index data.
3. Render commands cross to the render thread; material draws refer to a material render proxy.
4. The RHI records platform graphics commands and binds the generated material shader.
5. The GPU runs vertex processing, rasterization, and pixel processing into Slate's current render
   target, normally the editor window backbuffer or an intermediate compositor target.
6. Presentation/composition makes that target visible. CPU execution was not blocked on every
   pixel unless an explicit synchronization point was introduced.

## Unreal-specific machinery

The virtual include path is registered when the plugin module starts. The material compiler reads
the `.ush` at shader compile time; the editor does not load that text every frame.

This course uses explicit HLSL from the first lesson. At this stage Unreal still owns the material
translation, vertex shader, Slate batching, and draw submission; the lesson takes ownership only of
the small pixel function. That keeps the first HLSL example visible without also introducing the
global-shader and render-pass APIs.

These lessons do not author an RDG pass. RDG is Unreal's declarative lifetime and dependency system
for render passes and resources. Slate already owns the rendering path used here, which ultimately
reaches the RHI. A later lesson can take ownership of an RDG pass when that ownership itself is the
subject.

## Performance notes

Both examples submit constant-size geometry. The material version permits richer per-pixel work,
so its cost grows with covered pixels and shader complexity rather than with a CPU shape count.
Slate can batch draws only when their state is compatible; changing materials can split batches.

## Things deliberately not abstracted yet

There is no custom render target, scene proxy, global shader, explicit RDG graph, or render-thread
callback. The comparison keeps the first boundary—Slate submission versus shader execution—clear.

## Exercises

1. Change the material gradient to run vertically without changing the quad.
2. Add a third colour stop in shader math and compare it with a third Slate gradient stop.
3. Add a checker pattern using only UVs and `frac`.
4. Use Slate Insights or RenderDoc to find the corresponding draw calls.
