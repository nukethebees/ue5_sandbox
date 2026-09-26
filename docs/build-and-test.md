# Build and test

The root CMake project is the supported entry point for Unreal builds, editor tests, commandlets,
and native targets. Do not invoke UBT, `RunUBT.bat`, or `Build.bat` directly.

## First-time or worktree setup

Install CMake 4.4.2 or newer and Ninja, set `UE_ROOT` to the Unreal Engine installation, then
initialize the worktree:

```powershell
. .\dev.ps1
csetup
```

`csetup` synchronizes submodules, regenerates presets, installs the per-user jobserver when
needed, and prepares the DebugGame and Development worktrees. Its CMake workflows build their
configuration-local C# host-tool dependencies on demand. Use `csetup native` when only the native
toolchain is needed; it avoids Unreal worktree preparation. See the
[PowerShell guide](../PowerShell/README.md) for the other session commands.

Alternatively, set `UE_ROOT` in the ignored `CMakeUserPresets.json` using a local configure preset
that inherits from `development`. CMake builds pinned native dependencies from source; no package
manager configuration is needed.

Windows native builds and Unreal share the `CompilerVersion` and `WindowsSDKVersion` pins in
`Config/DefaultEngine.ini`. Install those MSVC x64 build tools and Windows SDK versions through
Visual Studio Installer. Both CMake toolchains use the pinned headers and libraries, including
when run from a developer prompt for a different toolset. After changing either pin, remove the
affected `out/build/<preset>` directories and rebuild the native libraries before building Unreal.

Project-owned build options and compile definitions use the `IOJ_` prefix. CMake passes
`IOJ_NATIVE_TOOLCHAIN` to UnrealBuildTools, project-file generation, and UAT; Unreal module rules
read that same environment variable. Set `IOJ_WITH_UNREAL=OFF` for standalone native builds.

## Development validation

Use the cheapest tier that validates the changed boundary. Native code is the normal inner loop;
Unreal is an integration boundary. The final integration planner selects the relevant gate from
the changed component graph; the normal DebugGame game/native workflow is reserved for
Unreal-facing or cross-cutting candidates.

### Clean task start

Keep the clean-build policy: load `dev.ps1`, remove this worktree's old build directories,
regenerate presets and code, then build once:

```powershell
python cmake/presets/generate.py
cmake --workflow --preset generate-code
cmake --workflow --preset task-start
```

`task-start` builds native tests (including the soak), native developer-tool tests, all nine C#
test assemblies, and generated-output consistency checks. It executes no tests and uses no
Unreal resources. Binaries and C# intermediates are isolated under `out/build/native`.
The `check-generated-code` build target owns the committed codegen fixture consistency check.

### Iteration: rebuild affected targets, then select tests

```powershell
cmake --build --preset native --target native-simulation-tests
ctest --test-dir out/build/native -L '^native-simulation$' -LE 'soak|compile-contract' --output-on-failure

cmake --build --preset native --target csharp-CodeFormatTools-build
ctest --test-dir out/build/native -L '^formatting$' --output-on-failure
```

CTest never rebuilds the C# assemblies. Each registered project uses `dotnet test --no-build
--no-restore`; a source fingerprint rejects stale binaries with the precise rebuild target.
Adding/removing files and changing transitive C# dependencies also invalidate the fingerprint.
Use `csharp-tests-build` after shared C# infrastructure changes. The canonical AgentGit installer
still performs its real private validation build and security tests.

Labels are regular expressions, not shell globs. One `-L` selects any matching label; repeated
`-L` options require every expression to match. `-LE 'soak|compile-contract'` excludes either
category. `ctest --test-dir out/build/native -N -L <label>` previews selection without executing.

C# labels include `csharp`, `agent-git`, `installer`, `architecture`, `benchmark`, `formatting`,
`game-package`, `git`, `native-binary`, and `unreal-build`. The `developer-tool` label includes
all standalone C# projects, Rust tests, and the registered layout, image-lab, and perf tests.
Mixed integration assemblies carry `integration;subprocess`; pure assemblies carry `unit`.
Labels apply to whole executables/assemblies, not individual GTest/MSTest cases.
The `native` label describes product/library validation, not implementation language. Layout
planner and image lab belong to `developer-tool`; their consumed layout/image libraries route
to the relevant tool explicitly. Image lab also carries `integration` for real file round trips.

`cmake --workflow --preset native-simulation-tests` builds and runs only ordinary simulation
tests. Use `native-simulation-full-tests` for ordinary tests plus soak, or
`native-simulation-soak-tests` for the soak alone. Both executables share simulation ABI,
iterator, compiler, and configuration policy.

### Native tidy scopes

Use `cmake --workflow --preset clang-tidy-simulation` (or `core`, `layout`, `lispb`, `memory`,
`level-authoring`, `s7`, `image`, `mesh-gen`) for scoped readiness. After its initial configure,
`cmake --build --preset clang-tidy-simulation` reruns that scope without a broad build.
Generated prerequisites remain dependencies of the relevant tidy targets. Inspect the resulting
`clang-tidy-<scope>.log` and resolve every diagnostic; the checks themselves are unchanged.

Implementation-only changes use their owning scope. Shared headers require consumer analysis:
follow `target_link_libraries` and actual include users, including header-only consumers. Core,
memory, profiling, compiler defaults, or uncertain cross-cutting changes require the full
`cmake --workflow --preset win-x64-clangcl-debug-tidy` sweep. Simulation public headers also
require level-authoring; level-authoring headers require simulation; image headers require
mesh-gen where consumed. Expand further for actual includes, and validate Unreal adapters when
those public interfaces cross the engine boundary. Directory ownership alone is insufficient.

For opt-in check profiling, use the prepared compilation database for a representative file:
`clang-tidy -p out/build/win-x64-clangcl-debug/clang-tidy --enable-check-profile <source.cpp>`.
Add `--store-check-profile=out/tidy-profile` to save JSON timing data. This preserves the normal
check set; the installed `run-clang-tidy` driver does not forward these profiling options.

### Final validation

```powershell
cmake --workflow --preset native-tests
cmake --workflow --preset tool-tests
```

Run only the affected broader validation classes once against the final candidate. Native final
validation includes the separate `native-simulation-soak-tests` and all compile-contract tests;
focused simulation final validation uses `native-simulation-full-tests`. Codegen/type-system final
validation includes `compile-contract`. Those nested builds retain a shared CTest resource lock.
The full native workflow excludes standalone layout-planner and image-lab tests. It is not the
repeated inner loop.

`tool-tests` builds its prerequisites before running the per-project tests, with no duplicate
umbrella C# test. Use it for shared/unknown tool infrastructure or broad tool validation. Known
C# tools select explicit projects; GitSupport expands to AgentGit, AgentGitInstaller, and GitTools.
Layout planner and image lab select their native workflows, jobserver its dedicated gate, Rust
its own CTest label, and perf its benchmark validation. Unknown tool paths retain broad tool and
native validation. CMake owns physical test registration in `cmake/csharp_tests.cmake`; the
`.integration-gates.json` owns paths, gates, affected consumers, and `testProjects`. AgentGit
loads it from the pinned base commit and unions it with an immutable copy embedded in the
installed executable as its minimum safety policy. No policy is loaded from the feature worktree.
AgentGit implementation changes also validate its installer consumer; AgentGit test-only edits
retain the dedicated test gate. Older pinned manifests without project metadata widen C# coverage.

CMake configure cross-checks registered projects against `Tools.slnx`, transitive MSBuild
references, and manifest ownership. `CMakeChecks` checks generated presets, configures `native`,
and runs the `cmake` infrastructure regressions, including `CMake.CSharpTests`. It excludes
`CMake.Presets` from that CTest invocation because the same check already ran before configure.
Shared C# build files and routing changes select this gate automatically. Python validation
covers all repository-owned Python under `Scripts` and `cmake` with Ruff and Pyright.

Do not rebuild Unreal merely because a native implementation has a thin Unreal adapter. Settle
the native behavior with the smallest target and test subset first.

### Focused Unreal integration

Use Unreal validation once an Unreal-facing boundary needs checking: module/build definitions,
reflection or UObject lifetime, engine adapters/APIs, UI, assets, or editor behavior.

```powershell
# Build the smallest broad cross-layer unit prerequisite, then run unit taxonomy tests.
cmake --workflow --preset debug-game-unit-tests

# Prepare and build an Editor-ready configuration when interactive validation is needed.
cplay debug-game
```

`debug-game-unit-tests` includes an Editor build and is not a native inner-loop command. For a
single integration concern, prefer a focused CMake target and CTest name/label filter over this
workflow.

### Merge-ready integration

For Unreal-facing or cross-cutting game changes, after implementation and rebase onto current `dev`, run:

```powershell
cmake --workflow --preset debug-game-tests
```

This is the normal DebugGame game/native integration gate. If it finds one native failure, return to that
target's focused native build/test loop, then rerun this gate once on the final HEAD.

Use `cmake --workflow --preset debug-game-full-tests` only for explicitly requested broad
validation; it also includes standalone developer-tool tests.

Use `cmake --build --preset debug-game --target run-editor` to build and launch the Editor, or
`run-editor-debug` to break at startup for an attached debugger. Regenerate Visual Studio project
files with `cprojectfiles` after module, plugin, target, or build-rule changes.

Live Coding is disabled by tracked project configuration and setup normalizes the local Editor
setting. Use the generated `Sandbox` solution with `DebugGame Editor | Win64` or
`Development Editor | Win64` for Visual Studio debugging.

`csetup` creates the worktree dependencies required by game and editor targets, including native
memory, image, material-generation, mesh-generation, CPU-feature, and generated-code artifacts.
DebugGame setup can import the shared audio assets when `BEE_AUDIO_ROOT` points to the
`sci-fi_ds_2220mb` pack. Run `cmake --workflow --preset import-game-audio` to invoke that import
separately; it succeeds without replacing assets when the source pack is unavailable.

## Packaging and asset maintenance

Use the CMake workflows for iterative staged and Development packages:

```powershell
cmake --workflow --preset development-staged-game
cmake --workflow --preset development-package
```

Use the full Shipping package path with:

```powershell
pwsh -NoProfile -File PowerShell/PackageGame.ps1
```

The `resave-assets` workflow modifies assets, so ensure intended files are writable before running
it.

CTest labels separate taxonomy from integration cost: native tests carry `native` plus their
existing unit/subsystem labels, while Unreal Automation tests retain their semantic labels.
Use the native presets rather than the generic `unit` label for the everyday native path. If Unreal
tests report missing or stale project/plugin modules, build the `editor` target through the matching
CMake preset before rerunning them. UBT owns target receipts, module manifests, and BuildIds; do not
edit or synchronize that metadata manually.

Run the focused configuration transition regression with:

```powershell
pwsh -NoProfile -File PowerShell/TestUnrealEditorConfigurationTransition.ps1
```

It takes one exclusive engine lease, then builds and smoke-tests `SandboxEditor` as DebugGame,
Development, then DebugGame again without asserting any particular BuildId value. Holding the
parent lease prevents another worktree from invalidating one step's UBT metadata before its smoke
reader starts.

## Coordination

CMake-managed Unreal builds take the repository's canonical engine gate exclusively. Unattended
editor tests and commandlets hold a compatible shared claim for their lifetime; interactive managed
editor launches hold it exclusively because Unreal can prompt to compile missing modules during
startup. The per-user jobserver also coordinates costly work across worktrees. Load `dev.ps1` and
run `get-jobserver-state` to inspect it. Do not overlap manually launched Editors, Visual Studio
builds, Live Coding, or direct UBT work with a managed Unreal build.

Normal project builds intentionally do not pass `-NoEngineChanges`. Project-owned runtime
dependencies such as `SandboxTracyClient.dll` are staged into the Editor target's engine output
directory, which that option treats as an engine change. Correctness instead comes from routing the
entire UBT process tree through the exclusive canonical engine claim.

See [the CMake guide](../cmake/README.md) for preset structure and [the native guide](../native/README.md)
for standalone-only workflows. Use [Benchmarks](benchmarks.md) for exclusive performance
measurements and [Profiling](profiling.md) to capture native level benchmarks with Tracy.
