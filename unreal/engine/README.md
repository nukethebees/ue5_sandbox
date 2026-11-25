# Patched Unreal Engine 5.8.4

Sandbox uses Epic's `5.8.4-release` at `f45729c0b6f097038e72b52d366a62baeca3275b`. The first patch lets SandboxHost opt into Slate activation. The second supplies the SandboxISMC instance-input hook. The patched Git tree is `31a46918d435f5c93a95acf130fdd7524ca24422`.

Run these PowerShell commands from the game repository with a clean Epic source checkout at `$engine`. The checkout must contain the release tag.

```powershell
$engine = 'C:/dev/UE5.8.4'
$series = (Resolve-Path 'unreal/engine/patches/5.8.4').Path
git -C $engine rev-parse '5.8.4-release^{commit}'
git -C $engine switch -c ioj/ue-5.8.4 f45729c0b6f097038e72b52d366a62baeca3275b
git -C $engine am "$series/0001-Allow-UELibrary-hosts-to-opt-into-Slate-window-activ.patch" "$series/0002-Expose-custom-instance-inputs-to-the-local-vertex-fa.patch"
git -C $engine rev-parse 'HEAD^{tree}'
```

Compare the release commit and patched tree with the IDs above. Register the checkout under `{92EDACB7-4E6D-41B6-61C9-6CB71C3CD7E3}` in `HKCU:\Software\Epic Games\Unreal Engine\Builds` and point `UE_ROOT` at it.

The game configuration requires the engine's Development UnrealPak executable. If it is missing after setting up the Epic source checkout, build it from the game repository before configuring.

```powershell
cmake -E env coj unreal-build --build-script "$engine/Engine/Build/BatchFiles/RunUBT.bat" --target UnrealPak --platform Win64 --configuration Development --project Sandbox.uproject --native-toolchain clang-cl
```

When migrating a worktree that built against an older engine, move the project's and plugins' generated `Intermediate/Build/Win64` directories aside while builds are stopped. Unreal can otherwise reuse precompiled headers that reference the old checkout. Reconfigure each Unreal CMake build directory with `-DUE_ROOT=C:/dev/UE5.8.4`, then regenerate IDE files with `coj unreal project-files --build-dir out/build/debug-game`.

To export the commits again from the maintained engine branch, use the same variables and run

```powershell
git -C $engine format-patch --no-signature -o $series 5.8.4-release..ioj/ue-5.8.4
```
