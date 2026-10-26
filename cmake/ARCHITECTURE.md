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

Pure CMake self-tests in `cmake/tests/` run during configure. ASAN runtime staging is a
dependency of instrumented targets, so a deleted staged DLL is restored on rebuild.
