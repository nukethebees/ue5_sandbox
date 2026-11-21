# Image Lab

Run `cmake --workflow --preset image-lab`. Open `out/build/win-x64-clangcl-debug/tools/image_lab/gui/image-lab.exe`, select a preset, preview the channels or tiling, and export a PNG. Import it into Unreal through the Content Browser.

For batch generation, run from the repository root:

```powershell
.\out\build\win-x64-clangcl-debug\tools\image_lab\cli\image-lab-cli.exe generate nebula_soft --output Saved/ImageLab
```
