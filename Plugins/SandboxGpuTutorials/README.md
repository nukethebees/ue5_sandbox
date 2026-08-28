# Sandbox GPU Tutorials

Open the Unreal Editor and choose **Tools > Sandbox GPU Tutorials**. The showcase is a native
Slate Nomad tab; it does not require a map, PIE, or production game modules.

The first three lessons use UI-domain materials with Custom expressions. The expressions include
the readable shader functions under `Shaders/`. Explicit HLSL is foundational to this course: the
early lessons keep Unreal's draw submission conventional while making the pixel work directly
inspectable. They deliberately stop short of custom global shaders, explicit RDG passes, buffers,
or compute dispatches.

The showcase has buttons that open the selected lesson's README and HLSL source. After editing a
`.ush`, use **Recompile Changed Shaders** in the editor (`Ctrl+Shift+.`) and allow compilation to
finish before judging the preview. Material asset edits still require applying and saving the
material.

The normal automation suites use NullRHI and validate serialized material contracts without a GPU.
After building the editor, run the explicit graphics-RHI check whenever tutorial HLSL changes:

```text
cmake --build --preset debug-game --target gpu-tutorial-shader-tests
```

The target uses an in-memory derived-data cache so this focused check does not depend on the local
Zen cache service.

Lesson documentation:

1. [GPU Rendering Mental Model](Docs/Lesson01/README.md)
2. [Shader Coordinates and Analytic Shapes](Docs/Lesson02/README.md)
3. [Parameters and Animation](Docs/Lesson03/README.md)
