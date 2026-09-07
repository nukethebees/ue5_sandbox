# Unreal Engine 5.8 vcpkg support

Date: 2026-09-07

## Decision

Do not use UnrealBuildTool's `AddVcPackage` API for project-managed dependencies. In UE 5.8 it
only consumes Epic's prebuilt `Engine/Source/ThirdParty/vcpkg` layout; it does not run vcpkg, read
the project's manifest, or support a configurable install root.

The project exposes `cpu-features` through a dedicated external Unreal module backed by this
repository's vcpkg install tree. The module owns the include path, library path, compile
definitions, platform checks, and CRT checks; `SpaceGame` consumes it as a private dependency.

## UnrealBuildTool API

The implementation is in:

```text
$UE_ROOT/Engine/Source/Programs/UnrealBuildTool/Configuration/Rules/ModuleRules.cs
```

`IsVcPackageSupported` is only a platform whitelist:

```csharp
public bool IsVcPackageSupported => Target.Platform == UnrealTargetPlatform.Win64 ||
            Target.Platform == UnrealTargetPlatform.Linux ||
            Target.Platform == UnrealTargetPlatform.LinuxArm64 ||
            Target.Platform == UnrealTargetPlatform.Mac;
```

It does not check whether vcpkg is installed, whether a requested package exists, whether an
architecture has prebuilt artifacts, or whether the compiler and CRT are compatible.

`AddVcPackage` still exists with this public signature:

```csharp
public void AddVcPackage(string packageName, bool addInclude, params string[] libraries)
```

It calls `GetVcPackageRoot`, verifies the package directory and named library files, adds the
libraries to `PublicAdditionalLibraries`, and optionally adds the package's `include` directory to
`PublicSystemIncludePaths`. It accepts no install-root or triplet argument.

`GetVcPackageRoot` constructs:

```text
ThirdParty/vcpkg/<UnrealPlatform>/<triplet>/<package>_<triplet>
```

UBT sets its working directory to `Engine/Source` in:

```text
$UE_ROOT/Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool.cs
```

The resulting package root is therefore always under:

```text
$UE_ROOT/Engine/Source/ThirdParty/vcpkg
```

`GetVcPackageRoot` is not virtual, and UBT does not consult `VCPKG_ROOT`,
`VCPKG_INSTALLED_DIR`, `VCPKG_TARGET_TRIPLET`, a project manifest, or CMake package metadata.

## What Epic uses it for

The only current `AddVcPackage` consumers in the complete Engine tree are:

```text
$UE_ROOT/Engine/Source/ThirdParty/zstd/zstd.Build.cs
$UE_ROOT/Engine/Source/ThirdParty/LibreSSL/LibreSSL.Build.cs
```

Their usage is limited to calls such as:

```csharp
if (!IsVcPackageSupported)
{
    return;
}

AddVcPackage("zstd", true, "zstd");
```

The `Vcpkg` external module is empty and explicitly records that automatic building is not
implemented:

```text
$UE_ROOT/Engine/Source/ThirdParty/vcpkg/Vcpkg.Build.cs
```

```csharp
// TODO: Support building vcpkg libraries automatically during a build.
```

Separate maintenance scripts under the same directory clone vcpkg 2022.03.10 and populate the
Engine's prebuilt layout. They are not invoked by UBT. The Windows script uses vcpkg's package
staging root rather than the conventional manifest-mode installed-tree layout.

Epic's GeoReferencing plugin is a useful counterexample. Its vcpkg-built PROJ dependency bypasses
`AddVcPackage` and explicitly consumes a plugin-local install tree in:

```text
$UE_ROOT/Engine/Plugins/Runtime/GeoReferencing/Source/ThirdParty/Proj.Build.cs
```

```csharp
PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, VcPkgInstalled, Triplet, "include"));
PublicAdditionalLibraries.Add(Path.Combine(LibPath, "proj.lib"));
```

No newer UE 5.8 vcpkg API replaces `AddVcPackage`. The normal abstraction for a project-owned
third-party library remains an external `ModuleRules` module.

## Supported mappings

The API produces the following triplets and only consumes release static libraries:

| Unreal target | Triplet | Notes |
|---|---|---|
| Win64 x64, default CRT | `x64-windows-static-md-v142` | Static library, dynamic CRT (`/MD`) |
| Win64 x64, `bUseStaticCRT` | `x64-windows-static-v142` | Static library and static CRT (`/MT`) |
| Win64 Arm64 | Corresponding `arm64-...-v142` triplet | The mapping exists, but this Engine checkout has no Arm64 package directories |
| Linux | `x86_64-unknown-linux-gnu` | Static release library built with Epic's custom Linux toolchain and libc++ |
| LinuxArm64 | `aarch64-unknown-linux-gnueabi` | Static release library built with the custom cross-toolchain |
| Mac | `x86_64-osx` | Architecture is hard-coded to x86_64 |

Windows always uses the `v142` triplet suffix regardless of the compiler selected by the UE 5.8
target. Android, iOS, consoles, and native Mac Arm64 are not supported by this API. It also has no
separate path for targets that opt into the debug CRT.

## Adopted project layout

`cpu-features` is a top-level manifest dependency because both Unreal and the native kernel
benchmark consume it. A hidden `unreal` CMake configure preset sets the install root for every
Unreal configuration to:

```text
vcpkg_installed/x64-windows
```

This repository-root tree is stable across Unreal configurations and is ignored by Git. UBT links
from it without depending on a particular CMake build directory.

The `codegen` and `kernel-benchmark` presets intentionally continue to inherit the base preset
directly. Their vcpkg packages remain in their per-build install trees, such as:

```text
out/build/codegen/vcpkg_installed/x64-windows
out/build/kernel-benchmark/vcpkg_installed/x64-windows
```

Manifest-mode vcpkg reconciles an install tree to the active dependency graph. Keeping the
code-generator, benchmark-feature, and Unreal trees independent prevents one configuration's
manifest features from removing or replacing packages required by another.

The current `cpu-features` port forces static library linkage. For `x64-windows` it installs:

```text
include/cpu_features/cpuinfo_x86.h
lib/cpu_features.lib
debug/lib/cpu_features.lib
```

Its CMake target also exports metadata that `AddVcPackage` would ignore:

```cmake
INTERFACE_COMPILE_DEFINITIONS "STACK_LINE_READER_BUFFER_SIZE=1024"
INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include/cpu_features"
IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/cpu_features.lib"
```

The release library uses the dynamic release CRT and is compatible with normal Unreal targets,
including the default Unreal Debug configuration, which ordinarily continues to use the release
CRT. A target using `bUseStaticCRT` or `bDebugBuildsActuallyUseDebugCRT` needs a matching vcpkg
triplet or library configuration.

## Unreal module behavior

`Plugins/SpaceGame/Source/CpuFeatures/CpuFeatures.Build.cs` is a dependency-only external module;
it is discovered through the plugin's `Source` directory and does not need a `.uplugin` module
entry. On Win64 x64 it defines `WITH_CPU_FEATURES=1` and exports:

```text
vcpkg_installed/x64-windows/include/cpu_features
STACK_LINE_READER_BUFFER_SIZE=1024
vcpkg_installed/x64-windows/lib/cpu_features.lib
```

A true Debug target that opts into the debug CRT uses
`vcpkg_installed/x64-windows/debug/lib/cpu_features.lib`. Other Unreal configurations use the
release library and dynamic CRT. The module stops the build with a targeted error when the
expected library is absent or the target requests a static CRT.

Other platforms and architectures define `WITH_CPU_FEATURES=0` without adding include or library
paths. They remain buildable and report SIMD detection as unavailable. No runtime DLL staging is
necessary because the vcpkg port builds a static library.

The game-instance subsystem queries `cpu_features::GetX86Info()` once during its existing platform
capability initialization. It copies the selected SSE, AVX, AVX-512, and AMX flags into
`FGameCapabilities::cpu_simd`; the main menu System page presents those flags in grouped rows.
