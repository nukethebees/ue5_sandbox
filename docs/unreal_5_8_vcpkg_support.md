# Unreal Engine 5.8 vcpkg support

Date: 2026-09-07

## Decision

Do not use UnrealBuildTool's `AddVcPackage` API for project-managed dependencies. In UE 5.8 it
only consumes Epic's prebuilt `Engine/Source/ThirdParty/vcpkg` layout; it does not run vcpkg, read
the project's manifest, or support a configurable install root.

Expose `cpu-features` through a dedicated external Unreal module backed by this repository's
vcpkg install tree. Keep the include path, library path, compile definition, platform checks, and
CRT checks in that module, then add it as a private dependency of `SpaceGame`.

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

## Project install-tree mismatch

The repository declares `cpu-features` only in the `kernel-benchmarks` manifest feature, and only
the `kernel-benchmark` CMake preset enables that feature. CMake currently creates a separate vcpkg
install tree for each build directory, for example:

```text
out/build/debug-game/vcpkg_installed/x64-windows
out/build/kernel-benchmark/vcpkg_installed/x64-windows
```

At the time of investigation, the DebugGame install tree did not contain `cpu-features`, and the
kernel-benchmark build directory had not been configured. A package-staging copy under the vcpkg
tool checkout is not a stable project install root and should not be linked by UBT.

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

## Recommended integration

When implementing the integration:

1. Move `cpu-features` to the manifest's top-level dependencies because it is no longer
   benchmark-only.
2. Configure a stable shared `VCPKG_INSTALLED_DIR`, such as the already ignored repository-root
   `vcpkg_installed`, so UBT does not depend on a particular CMake preset's build directory.
3. Add a `CpuFeatures` external module under the SpaceGame plugin. Initially support Win64 x64 and
   fail clearly for an absent artifact or incompatible CRT.
4. Export `include/cpu_features`, release `lib/cpu_features.lib`, and
   `STACK_LINE_READER_BUFFER_SIZE=1024` from that module.
5. Add `CpuFeatures` to `SpaceGame`'s private module dependencies.

No runtime DLL staging is necessary because this vcpkg port only builds a static library. This
keeps manifest ownership in vcpkg while isolating its filesystem layout from the gameplay module.
