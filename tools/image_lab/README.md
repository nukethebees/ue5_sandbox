# Image Lab

Image Lab is the standalone editor for the deterministic generators in `native/image`. It writes
RGBA PNGs only and never creates or imports Unreal assets.

## Build and run

Initialize the pinned SDL3 and Dear ImGui submodules, then build the dedicated workflow:

```powershell
git submodule update --init native/third_party/sdl native/third_party/imgui `
  native/third_party/nativefiledialog-extended
cmake --workflow --preset image-lab
```

Run the GUI from `out/build/win-x64-clangcl-debug/tools/image_lab/gui/image-lab.exe`. Its default output
directory is `Saved/ImageLab` at the repository root.

The CLI is at `out/build/win-x64-clangcl-debug/tools/image_lab/cli/image-lab-cli.exe`:

```powershell
image-lab-cli list
image-lab-cli describe nebula_soft
image-lab-cli generate nebula_soft --output Saved/ImageLab
image-lab-cli generate-all --output Saved/ImageLab
image-lab-cli dump-preset nebula_soft --output request.json
image-lab-cli generate-request request.json --output Saved/ImageLab
```

`dump-preset` writes a complete, readable JSON `GenerationRequest`, including every generator's
parameters and shared post-process values. Edit that file to make a custom request, then pass it to
`generate-request`. Generator, cellular-mode, and post-process-output fields use stable lowercase
names such as `domain_warped_noise`, `borders`, and `signed_distance`.

Select a default preset in the GUI to load its native request. Editing any value makes it custom;
the generator selector resets generator-specific values while retaining shared output shaping. Use
the preview channel and tiled-preview controls to inspect channels and seams before export.

To use an image in Unreal, import a generated PNG through the ordinary Content Browser workflow and
promote the resulting asset deliberately. Image Lab does not generate `.uasset` files.
