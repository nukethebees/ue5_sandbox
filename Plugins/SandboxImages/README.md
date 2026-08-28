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

In Unreal Editor, click **Image Lab** in the level-editor toolbar or choose
**Tools > Sandbox Image Lab**. The editor tab provides a transient preview, generator-specific
parameters, **Generate Selected**, **Generate All Defaults**, and **Open Output Folder**. Changing a
parameter refreshes the bounded preview without writing anything. Preview controls can display a
checkerboard-composited image, opaque RGB, or individual channels, and the 2x2 mode makes tiling
seams easy to spot. The editor console command remains available:

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

The reproducible parameters and default output table live in
`GenLab/Private/Generation/ImageGenerators.h` and `ImageGenerators.cpp`. The editor UI creates the
same generation requests used by batch regeneration; it does not define a separate preset format.
Change the output name and seed to create a deterministic variant without changing the canonical
default set. Shared invert and contrast controls shape the final intensity after generation. Noise
can optionally use periodic sampling so opposite edges match exactly; imported tileable noise uses
wrap addressing. Threshold shaping supports hard binary masks or a configurable smooth transition.
Curl-noise flow maps encode a normalized two-dimensional direction in RG, with neutral blue and
opaque alpha. Scalar output-shaping controls are intentionally not applied to flow-map data.

| Output | Channel meaning |
| --- | --- |
| `soft_radial_gradient.png` | White intensity duplicated into RGB and alpha |
| `ring_mask.png` | White intensity duplicated into RGB and alpha |
| `starfield.png` | Star intensity duplicated into RGB and alpha over transparency |
| `coherent_noise.png` | Grayscale data in RGB with opaque alpha |
| `nebula_soft.png` | Soft tileable domain-warped grayscale noise |
| `energy_filaments.png` | Contrasted, smoothly thresholded tileable energy structure |
| `shield_turbulence.png` | Broad tileable turbulent shield modulation |
| `nebula_flow.png` | Tileable normalized curl-noise direction encoded in RG |
| `shield_distortion_flow.png` | Denser tileable shield-distortion direction encoded in RG |
| `hex_grid_mask.png` | White intensity duplicated into RGB and alpha |

The generators reject invalid dimensions and generator-specific parameter ranges instead of
silently producing malformed output. Seeded outputs also have stable checksum tests so algorithm
changes are deliberate.

Each PNG is automatically imported as a `UTexture2D` in `/SandboxImages/Lab/Images`. Regeneration
updates the existing generated texture assets in place. The importer disables sRGB and uses mask
compression for masks or grayscale compression for coherent noise. It also applies explicit texture
group, mip, filtering, and addressing settings. Flow maps use vector-displacement compression to
preserve their RG vector data. Every generated asset records the generator version and parameters
in its package metadata.

## Unreal import guidance

These are technical textures rather than colour artwork. Disable **sRGB** when importing them for
shader data or masks, and choose mask/grayscale compression appropriate to the channels actually
used. Review mip generation and filtering for the intended effect, especially for the thin hex and
ring lines. Enable **Show Plugin Content** in the Content Browser to inspect the automatically
imported textures. Generated `.uasset` experiments must remain under
`Plugins/SandboxImages/Content/Lab/Images` until deliberately promoted into a normal committed
content location.
