# Build and test

CMake owns native and Unreal builds and ordinary test registration. coj exposes developer
operations, including editor launches and authored-asset commandlets, and builds their CMake prerequisites.

## First-time or worktree setup

Install CMake 4.4.2 or newer and Ninja, set `UE_ROOT` to the Unreal Engine installation, then
initialize the worktree:

```powershell
coj prepare-worktree
```

The maintainer installs central tools and adds the shared tool-link directory to PATH (see
[developer tools](../tools/README.md)). Preparation owns submodules, presets, and code generation.
For Unreal development, configure and build the required configuration with CMake as shown below.
CMake invokes `coj unreal-build` from PATH for Unreal build-script invocation,
and builds revision-local Rust tools on demand. See the
[PowerShell guide](../PowerShell/README.md) for installation and packaging scripts.

Alternatively, set `UE_ROOT` in the ignored `CMakeUserPresets.json` using a local configure preset
that inherits from `development`. CMake builds pinned native dependencies from source; no package
manager configuration is needed.

Windows native builds and Unreal share the `CompilerVersion` and `WindowsSDKVersion` pins in
`Config/DefaultEngine.ini`. Install those MSVC x64 build tools and Windows SDK versions through
Visual Studio Installer. Both CMake toolchains use the pinned headers and libraries, including
when run from a developer prompt for a different toolset. After changing either pin, remove the
affected `out/build/<preset>` directories and rebuild the native libraries before building Unreal.

`LLVM_ROOT` is an environment variable, not a project cache option. Set it before configuring
a build tree; leave it unset to use LLVM tools from PATH. A nonempty root selects tools
exclusively from its `bin` directory, and missing tools fail configuration. CMake captures the
selection during configure and passes it explicitly to child tests/tools, so later shell
environment changes do not change their selection. Set `$env:LLVM_ROOT` and configure again
to select another installation; `-DLLVM_ROOT=...` is ignored.
Unreal/UBT still compiles with MSVC and consumes the native `.lib`
files. The pinned Microsoft linker/SDK, `/MD` runtime and simulation `_ITERATOR_DEBUG_LEVEL=0`
remain shared ABI settings. Tidy uses the installed executable without LLVM development packages;
IOJ checks are optional built-in machine tooling. See [clang-tidy](clang-tidy.md).
The same root supplies `llvm-lib`, `llvm-nm` and `llvm-readobj` for native archives and mimalloc
symbol audits, plus `clang-scan-deps` for non-unity C++ dependency scanning.
Native ASAN requires compiler-rt's Windows runtime DLL, import library and runtime
thunk in the selected Clang resource directory. The explicit [LLVM toolchain build](../tools/llvm/README.md)
installs and verifies these dependencies; game workflows do not repair missing toolchain contents.
Run `cmake --workflow --preset win-x64-clangcl-debug-asan` for opt-in ASAN validation.
The cache option `IOJ_ASAN_WORKAROUND_LLVM_215376` defaults to `ON` and disables Windows ASAN
stack-use-after-return instrumentation because LLVM's `runtime` and `always` modes crash when
an exception leaves a catch handler
([LLVM #215376](https://github.com/llvm/llvm-project/issues/215376), reproduced with the pinned LLVM).
This is an upstream workaround, not normal project ASAN policy. When testing newer LLVM versions,
configure with `cmake --preset win-x64-clangcl-debug-asan -DIOJ_ASAN_WORKAROUND_LLVM_215376=OFF`
and rebuild to use the compiler's normal instrumentation. Configuration reports when the workaround is active.
Heap and stack bounds, use-after-free, and stack-use-after-scope checks remain enabled.
While the workaround is active, `ASAN_OPTIONS=detect_stack_use_after_return=1` cannot enable the
omitted instrumentation.

Project-owned build options and compile definitions use the `IOJ_` prefix. Build and project-file commands pass
`IOJ_NATIVE_TOOLCHAIN` to Unreal; Unreal module rules
read that same environment variable. Set `IOJ_WITH_UNREAL=OFF` for standalone native builds.

## Development validation

Use the cheapest tier that validates the changed boundary. Native code is the normal inner loop;
Unreal is an integration boundary. Select validation from the affected dependencies; reserve the
normal DebugGame game/native workflow for Unreal-facing or cross-cutting candidates.

### Clean task start

Begin each new task from anywhere in its Git worktree:

```powershell
coj prepare-worktree
```

The maintainer installs/updates `coj` with `pwsh -NoProfile -File PowerShell/InstallCoj.ps1`
and manages PATH; agents assume it is available. On a fresh setup, run
`coj prepare-worktree`, then `coj install-central-tools` for jobserver.
The install command only configures native and runs the jobserver installation target.
Agents report missing commands instead of installing them; `--version` allows manual diagnosis.
Preparation does not install missing tools.
See the [Rust tooling instructions](../tools/rust/README.md).
Preparation removes only the worktree-root `out` directory, synchronizes and initializes/updates
recursive submodules, invokes the same generator as `coj presets`, disables Live Coding in existing
saved Editor settings, then runs the `generate-code` CMake workflow. Each phase streams its output
and a failure stops preparation immediately.
It does not perform a broad project/test build. Build only the targets relevant to the task afterward.

For an explicit broad baseline, `cmake --workflow --preset task-start` remains available separately
from normal task preparation. It builds native tests (including the soak), native developer-tool
tests, all registered Rust tool test binaries, and generated-output consistency checks. It executes no tests and uses no
Unreal resources. Build outputs are isolated under `out/build/native`.
The `check-generated-code` build target owns the committed codegen fixture consistency check.

### Iteration: rebuild affected targets, then select tests

```powershell
cmake --build --preset native --target native-simulation-tests
ctest --test-dir out/build/native -L '^native-simulation$' -LE 'soak|compile-contract' --output-on-failure

ctest --test-dir out/build/native -L '^native-binary-tools$' --output-on-failure
```

Rust tool CTest entries invoke `cargo test --locked` for the selected package. Cargo handles
incremental rebuilds in the CMake build directory. `rust-tool-tests-build` prebuilds their tests.

Labels are regular expressions, not shell globs. One `-L` selects any matching label; repeated
`-L` options require every expression to match. `-LE 'soak|compile-contract'` excludes either
category. `ctest --test-dir out/build/native -N -L <label>` previews selection without executing.

Rust tool labels include `benchmark-tools`, `game-package-tools`, and `native-binary-tools`.
The `developer-tool` label includes these packages and the registered layout, image-lab, and perf tests.
Labels apply to whole executables or Cargo packages, not individual test cases.
The `native` label describes product/library validation, not implementation language. Layout
planner and image lab belong to `developer-tool`; their consumed layout/image libraries route
to the relevant tool explicitly. Image lab also carries `integration` for real file round trips.

`cmake --workflow --preset native-simulation-tests` builds and runs only ordinary simulation
tests. Use `native-simulation-full-tests` for ordinary tests plus soak, or
`native-simulation-soak-tests` for the soak alone. Both executables share simulation ABI,
iterator, compiler, and configuration policy.

### Native tidy scopes

Configure `win-x64-clangcl-debug-tidy`, then run `coj tidy --scope simulation
--build-dir out/build/win-x64-clangcl-debug/clang-tidy`. See [clang-tidy](clang-tidy.md) for the other scopes.

Implementation-only changes use their owning scope. Shared headers require consumer analysis:
follow `target_link_libraries` and actual include users, including header-only consumers. Core,
memory, profiling, compiler defaults, or uncertain cross-cutting changes require the full
`coj tidy --scope native --build-dir out/build/win-x64-clangcl-debug/clang-tidy` sweep. Simulation public headers also
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

`tool-tests` builds its prerequisites before running per-tool tests. Use it for shared/unknown
tool infrastructure or broad tool validation. Known tools use focused builds and labels.
Layout planner and image lab use their native workflows, jobserver its focused tests, and perf its
benchmark validation. coj uses `cargo test --package coj --locked` from `tools/rust`.

CMake owns Rust test registration in `tools/rust/CMakeLists.txt`. Run the `cmake` infrastructure
regressions when changing this wiring, including `CMake.RustHostTools`.

After required validation and explicit user authorization, run `coj integrate` for the
privileged coj Git transaction. This cheap operation needs no jobs-board ticket. Integration
performs pinned rebase, cheap sanity checks, atomic dev promotion, refresh and cleanup. It does
not select or rerun build/test gates. Resolve a conflicting final rebase with
`coj git rebase dev`, validate, and retry.
Validate shared tool build and registration changes with the CMake infrastructure checks.
Python validation covers repository-owned Python under `cmake` with Ruff and Pyright.

Do not rebuild Unreal merely because a native implementation has a thin Unreal adapter. Settle
the native behavior with the smallest target and test subset first.

### Focused Unreal integration

Use Unreal validation once an Unreal-facing boundary needs checking: module/build definitions,
reflection or UObject lifetime, engine adapters/APIs, UI, assets, or editor behavior.

```powershell
# Build the smallest broad cross-layer unit prerequisite, then run unit taxonomy tests.
cmake --workflow --preset debug-game-unit-tests

# Prepare and build an Editor-ready configuration when interactive validation is needed.
cmake --workflow --preset debug-game
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

After configuring `debug-game`, use `coj editor` to build and launch the Editor, or
`coj editor --wait-for-debugger`. Use `--build-dir <configured-tree>` for another configuration.
Generate Visual Studio projects with `coj unreal project-files` using `UE_ROOT`, or
`--ue-root <engine-root>`. No CMake configure is needed. To reuse configured engine/toolchain
settings, pass `--build-dir <configured-tree>` instead.

Live Coding is disabled by tracked project configuration. `coj prepare-worktree` disables it
once in saved Editor settings if the file exists; Editor launches also pass an explicit INI override.
Use the generated `Sandbox` solution with `DebugGame Editor | Win64` or
`Development Editor | Win64` for Visual Studio debugging.

To prepare dependencies and IDE projects without running the full build workflow:

```powershell
cmake --preset debug-game
cmake --build --preset generate-worktree-code-debug-game
cmake --build --preset worktree-dependencies-debug-game
coj unreal project-files --build-dir out/build/debug-game
```

Use `development` in place of `debug-game` for Development. The dependency target builds native
memory, image, material-generation, mesh-generation, CPU-feature, and generated-code artifacts.
To import shared audio assets, set `BEE_AUDIO_ROOT` to the `sci-fi_ds_2220mb` pack and run
`coj unreal import-game-audio --build-dir out/build/debug-game`. This optional command
builds its Editor prerequisite and succeeds without replacing assets when the source pack is unavailable.

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

Launch an existing staged game with `coj run-staged`; use `--build-dir out/build/shipping`
for Shipping. This does not cook or package.

Explicit authoring commands build `editor` and any additional material prerequisite first:

- `coj unreal resave-assets`
- `coj unreal import-game-audio`
- `coj unreal generate-scripted-level-assets`
- `coj unreal generate-slate-dsl-smoke-asset`
- `coj unreal generate-lab-mesh <box|cylinder|sphere|cone|hex-frame|hex-tile|honeycomb-panel|assemblies>`
- `coj unreal generate-ui-glow-material`
- `coj unreal generate-world-soft-target-assets`
- `coj unreal generate-migrated-materials`
- `coj unreal generate-celestial-analytic-material`
- `coj unreal generate-space-dust-material`

These default to the configured `out/build/debug-game` tree; pass `--build-dir` to select another.
Material description compilation remains in CMake. Asset commands update authored content.
Formatting uses `coj format --changed`, `--staged`, or `--all`, without configuring CMake.
Use `coj presets` to regenerate presets or `coj presets --check` to verify them.
The `CMake.Presets` test remains in the tool validation workflow and ordinary CMake test inventory.

CTest labels separate taxonomy from integration cost: native tests carry `native` plus their
existing unit/subsystem labels, while Unreal Automation tests retain their semantic labels.
Use the native presets rather than the generic `unit` label for the everyday native path. If Unreal
tests report missing or stale project/plugin modules, build the `editor` target through the matching
CMake preset before rerunning them. UBT owns target receipts, module manifests, and BuildIds; do not
edit or synchronize that metadata manually.

Run the focused configuration transition regression with:

```powershell
pwsh -NoProfile -File PowerShell/tests/TestUnrealEditorConfigurationTransition.ps1
```

Use the jobs-board request/check/start/end protocol with an exclusive ticket for this command.
It builds and smoke-tests `SandboxEditor` as
DebugGame, Development, then DebugGame again without asserting any particular BuildId value.

## Coordination

Use stock Codex and `coj jobs`. Cheap inspections need no ticket. Request a shared ticket
for heavyweight builds/tests, or an exclusive ticket for benchmarks and commands that modify
shared Unreal engine output. Check until Ready, start immediately before the ordinary command,
and end immediately when it returns, including failure. Tools and CMake do not schedule themselves.
See the [jobs-board workflow](../tools/jobserver/README.md).
Do not overlap manually launched Editors, Visual Studio builds, Live Coding, or direct UBT work
with a managed Unreal build. Scheduling is cooperative and does not track lingering processes.

Normal project builds intentionally do not pass `-NoEngineChanges`. Project-owned runtime
dependencies such as `SandboxTracyClient.dll` are staged into the Editor target's engine output
directory, which that option treats as an engine change. Request exclusive access for these builds.

See [the CMake guide](../cmake/README.md) for preset structure and [the native guide](../native/README.md)
for standalone-only workflows. Use [Benchmarks](benchmarks.md) for exclusive performance
measurements and [Profiling](profiling.md) to capture native level benchmarks with Tracy.
