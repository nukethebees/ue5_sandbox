# Lesson 03 — Parameters and Animation

## Goal

Control persistent GPU shader behavior from small CPU-side parameter updates while leaving geometry
unchanged.

## What you should understand after this lesson

- A material instance shares compiled shader code with its parent and supplies parameter values.
- Updating a scalar or vector does not rebuild Slate geometry or recompile the material.
- The editor thread changes UObject-side state; Unreal propagates render-facing values through its
  material-instance resource path rather than exposing a GPU pointer to UI code.
- Pausing CPU time updates freezes this shader because its time is explicit, not an engine Time node.

## Architecture

`SLesson03` owns a lesson-specific state object, a `UMaterialInstanceDynamic`, and its material
brush. The parent UI material includes `Shaders/Lesson03/Lesson03.ush`. Slider and colour-button
callbacks update parameters; widget Tick updates only `TimeSeconds` while animation is enabled.

## CPU-side code

The stable parameter contract is:

| Parameter | Type | Update cadence |
|---|---|---|
| `TimeSeconds` | scalar | Once per animated editor frame |
| `RingThickness` | scalar | When its slider changes |
| `AnimationSpeed` | scalar | When its slider changes |
| `PulseAmount` | scalar | When its slider changes |
| `PrimaryColor` | vector | When a colour button is pressed |

The lesson's strong references are necessary because `FSlateMaterialBrush` and the Slate renderer
do not keep the material UObject alive. Closing the tab releases the brush and roots; Unreal then
retires the associated render resources according to its normal cross-thread resource lifecycle.

## GPU-side code

The pixel shader derives repeated radial bands, a pulsing center, a translating scan line, and a
background grid from UVs and the five parameters. All of those visuals are evaluated per fragment;
there is still only one preview quad.

## Data flow

1. A Slate callback or Tick changes a value on the editor thread.
2. `UMaterialInstanceDynamic::Set*ParameterValue` searches the instance's game-thread override
   array by parameter name. If the value changed, it stores the new CPU value.
3. The setter enqueues a render command that captures the material render proxy, hashed parameter
   information, and parameter value. The call does not wait for that command to execute.
4. On the render thread, the command updates the proxy's parameter storage and calls
   `CacheUniformExpressions`. This makes the updated uniform-expression data available to later
   draws; it does not compile a new shader permutation.
5. When Slate's material batch is executed, the RHI binds the existing shader and its current
   parameter/uniform data.
6. GPU lanes evaluate the same shader code with the new values and write different pixels.

No vertex array containing rings, grid lines, or scan-line points crosses the boundary. The exact
driver-level upload and buffering strategy is owned by Unreal's uniform-expression/material system;
the observable contract is a small parameter update, not immediate synchronous GPU memory access.

## Unreal-specific machinery

The MID is CPU-visible UObject state plus a render proxy/resource representation consumed on the
rendering side. Parameter setters run on the editor thread, and their render commands copy the
parameter identity and value rather than retaining references to local callback variables. Draw
execution is deferred. Code must therefore keep the UObject alive; the lesson does so with a strong
object pointer because neither `FSlateMaterialBrush` nor the Slate renderer owns the material.
No explicit flush or fence is needed here because the material API establishes the game-thread to
render-thread handoff.

## Performance notes

The CPU work and transfer scale with a handful of values, while pixel cost scales with preview area
and shader complexity. Rebuilding a large CPU vertex array would instead add allocation, generation,
copy/upload, and potentially batching costs. Parameter updates are not free, so avoid repeatedly
setting unchanged values and avoid creating an MID per trivial element when many elements can share
state.

The lesson deliberately uses name-based setters because the parameter contract remains obvious.
For a hot path with many repeated updates, `InitializeScalarParameterAndGetIndex` followed by
`SetScalarParameterByIndex` avoids the repeated name lookup. Unreal already suppresses the render
command when a setter receives the same value currently stored by the MID.

## Things deliberately not abstracted yet

The state, controls, parameter names, and update calls remain in the lesson. There is no parameter
binding framework, material collection, custom uniform buffer, structured buffer, or RDG resource.

## Exercises

1. Add a toggle that freezes only the rings while the scan line continues.
2. Add a configurable ring count without adding geometry.
3. Replace the colour presets with a live colour picker.
4. Animate a rotating scan line with `atan2` or a dot product against a rotating direction.
5. Avoid calling a parameter setter when the new slider value equals the old value.
