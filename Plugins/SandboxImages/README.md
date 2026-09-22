# SandboxImages

`SandboxImages` remains a runtime plugin for image and texture experiments. Procedural image
generation lives in the standalone [Image Lab](../../tools/image_lab/README.md); it does not write
or import Unreal assets.

Generate PNGs with Image Lab, then deliberately import and promote any image needed by an Unreal
asset. The World Soft Target asset commandlet no longer regenerates its two texture inputs.
