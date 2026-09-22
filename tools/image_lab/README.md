# Image Lab

Image Lab is the standalone editor for the deterministic generators in `native/image`. It writes
RGBA PNGs only and never creates or imports Unreal assets.

## Build and run

Initialize the pinned SDL3 and Dear ImGui submodules, then build the dedicated workflow:

```powershell
git submodule update --init native/third_party/sdl native/third_party/imgui
cmake --workflow --preset image-lab
```

Run the GUI from `out/build/image-lab/tools/image_lab/gui/image-lab.exe`. Its default output
directory is `Saved/ImageLab` at the repository root.

The CLI is at `out/build/image-lab/tools/image_lab/cli/image-lab-cli.exe`:

```powershell
image-lab-cli list
image-lab-cli describe nebula_soft
image-lab-cli generate nebula_soft --output Saved/ImageLab
image-lab-cli generate-all --output Saved/ImageLab
```

Select a default preset in the GUI to load its native request. Editing any value makes it custom;
the generator selector resets generator-specific values while retaining shared output shaping. Use
the preview channel and tiled-preview controls to inspect channels and seams before export.

To use an image in Unreal, import a generated PNG through the ordinary Content Browser workflow and
promote the resulting asset deliberately. Image Lab does not generate `.uasset` files.
