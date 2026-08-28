# SandboxImages

`SandboxImages` is a thematic sandbox for focused image and texture work. The runtime module is the
small core of the plugin; it should only gain reusable functionality when an experiment has proved
useful.

`GenLab` contains deliberately small, disposable procedural-generation experiments. Generator code
is committed so results are reproducible, but GenLab itself should remain easy to rewrite or remove.
If a generator grows into reusable infrastructure, promote or refactor it instead of turning GenLab
into a permanent dumping ground.

`Plugins/SandboxImages/Content/Lab/Images` contains generated results and is ignored by Git.
Everything there is disposable and safe to regenerate. PNGs and any Unreal-generated `.uasset`
files in that directory are output, not source assets. If an image becomes useful, deliberately copy
or promote it into an appropriate normal committed content location.

## Regenerating the examples

From the repository root, the preferred command is:

```text
cmake --build --preset debug-game --target generate-lab-images
```

This builds the editor if needed, selects the correct configuration-specific executable, and uses a
project-local derived-data cache under the ignored build directory.

In Unreal Editor, click **Generate Lab Images** in the level-editor toolbar. The same action is also
available under **Tools > Generate Sandbox Lab Images**. A notification reports success or directs
you to the Output Log when a write fails. The editor console command remains available:

```text
SandboxImages.GenerateLabImages
```

For headless regeneration, run the `GenerateSandboxImages` commandlet through the project's normal
Unreal Editor command-line executable:

```text
UnrealEditor-Cmd.exe Sandbox.uproject -run=GenerateSandboxImages -unattended -nop4 -nullrhi -nosound
```

Either operation creates `Plugins/SandboxImages/Content/Lab/Images` when needed and regenerates the
complete current set of PNGs, then imports and saves matching Unreal texture assets beside them. It
logs every written file and reports failures in the Output Log or process exit code.

## Current outputs

The reproducible parameters live in `GenLab/Private/Generation/ImageGenerators.h`, with the current
output set kept as explicit calls in `LabImageWriter.cpp`.

| Output | Channel meaning |
| --- | --- |
| `soft_radial_gradient.png` | White intensity duplicated into RGB and alpha |
| `ring_mask.png` | White intensity duplicated into RGB and alpha |
| `starfield.png` | Star intensity duplicated into RGB and alpha over transparency |
| `coherent_noise.png` | Grayscale data in RGB with opaque alpha |
| `hex_grid_mask.png` | White intensity duplicated into RGB and alpha |

The generators reject invalid dimensions and generator-specific parameter ranges instead of
silently producing malformed output. Seeded outputs also have stable checksum tests so algorithm
changes are deliberate.

Each PNG is automatically imported as a `UTexture2D` in `/SandboxImages/Lab/Images`. Regeneration
updates the existing generated texture assets in place. The importer disables sRGB and uses mask
compression for masks or grayscale compression for coherent noise.

## Unreal import guidance

These are technical textures rather than colour artwork. Disable **sRGB** when importing them for
shader data or masks, and choose mask/grayscale compression appropriate to the channels actually
used. Review mip generation and filtering for the intended effect, especially for the thin hex and
ring lines. Enable **Show Plugin Content** in the Content Browser to inspect the automatically
imported textures. Generated `.uasset` experiments must remain under
`Plugins/SandboxImages/Content/Lab/Images` until deliberately promoted into a normal committed
content location.
