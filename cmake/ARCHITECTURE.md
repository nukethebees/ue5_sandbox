# Build layer ownership

UBT and Unreal `.Target.cs` / `.Build.cs` files own Unreal compilation. CMake orchestrates
their invocation and native prerequisites; `unreal/build/` registers editor, game,
packaging and generated-input targets. Reusable helpers live in `cmake/unreal.cmake`.
`coj` owns worktree preparation, launching, project files, formatting and asset operations.

`cmake/presets/generate.py` generates `base.json`, `native.json`, `unreal.json` and
`native-benchmarks.json`. Regenerate with `coj presets`; verify with `coj presets --check`.
Machine-specific values belong in ignored `CMakeUserPresets.json`.

Presets select configuration, unity, sanitizers and profiling. Toolchains discover
pinned compilers/SDKs and select the CRT. Standalone native settings follow
`CMAKE_BUILD_TYPE`; Unreal configurations map to native build types and stage native
artifacts for UBT. Runtime Tracy selection remains configuration-specific for Unreal.

`sandbox::native_defaults` bundles language standard, conformance, ABI, RTTI, sanitizers
and optional compiler build profiling. Native libraries expose it; executables consume
it privately. Clang uses C++26; pinned MSVC presets use C++23 because CMake cannot select
their C++26 mode. UBT owns its separate language standard. First-party warnings stay
private; required MSVC conformance switches remain enabled independently of warnings.
Warnings-as-errors use `COMPILE_WARNING_AS_ERROR`, overridable through CMake's standard
cache variable or `--compile-no-warning-as-error` switch.

Built executable arguments use `"$<TARGET_FILE:target>"` with appropriate dependencies;
CMake resolves a bare target name only in command position. Rust host tools expose
imported targets through `rust_host_tools.cmake`; Cargo owns incremental tracking and
outputs stay under the build directory.

Lispb's Release generator is shared by compatible presets within each worktree under
`out/host-tools/lispb/<identity>/<configuration>`. `lispb_host_tool.cmake` identifies
the host compiler/toolchain and forwarded host settings, independently of the game's
configuration, unity and sanitizer settings. Compiler path, version, size and modification
time participate in the identity; build profiling selects a separate host directory.
The generated `build_lispb_host.cmake` driver locks that directory across configuration
and incremental building. Ninja owns source/header dependency tracking; there is no
outer source glob or success stamp used to skip its stale-file check.

`cmake --workflow --preset lispb-host` prepares the shared generator directly.
Its `IOJ_LISPB_ONLY` mode configures just the driver. The driver's internal
`IOJ_LISPB_HOST_BUILD` mode defines the actual executable. The host uses the normal
native configuration with Unreal/developer applications disabled, and builds only
`--target lispb`; Ninja selects its library dependencies. Unrelated native targets
are configured but not compiled. The persistent directory retains configure results.
Normal builds and `coj prepare-worktree` reach the same driver through their existing
code-generation dependencies. A new game build directory reuses the host; changing
Lispb sources rebuilds the affected host files. Lispb test libraries in the outer
configuration remain independent. Clean compilation profiles must account for the
host directory being outside the outer build tree; a fresh outer directory alone no
longer measures a fresh host build.

Pure CMake self-tests in `cmake/tests/` run during configure. ASAN runtime staging is a
dependency of instrumented targets, so a deleted staged DLL is restored on rebuild.
