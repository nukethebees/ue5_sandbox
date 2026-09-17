# Native clang-tidy

The native clang-tidy workflow is opt-in. It uses clang-cl and LLVM's `run-clang-tidy` to audit
every eligible native `.cpp` entry in the preset's `compile_commands.json`. Unreal Engine,
generated code, third-party dependencies, and vendored libraries are excluded.

## Run clang-tidy

Configure and audit the native compilation database:

```powershell
cmake --preset win-x64-clangcl-debug-tidy
cmake --build --preset win-x64-clangcl-debug-tidy
```

This configures a dedicated build tree at `out/build/win-x64-clangcl-debug/clang-tidy`. It is
separate from the ordinary clang-cl configuration because it disables precompiled headers and C++
dependency scanning for a clang-tidy-compatible compilation database.

Or run both steps together:

```powershell
cmake --workflow --preset win-x64-clangcl-debug-tidy
```

To rerun tidy after an unchanged build, clean that preset's build tree first:

```powershell
cmake --build --preset win-x64-clangcl-debug-tidy --target clean
cmake --build --preset win-x64-clangcl-debug-tidy
```

The tidy preset does not make tidy findings fatal and does not apply fixes automatically. It
disables precompiled headers for the audit configuration so every compilation-database entry can be
analyzed independently. It also disables C++ dependency scanning so the database does not refer to
build-only module-map response files. Diagnostics print to the CMake build output and are saved by
default to `out/build/win-x64-clangcl-debug/clang-tidy/clang-tidy.log` (overwritten on each audit).

The audit uses `run-clang-tidy`'s detected CPU count by default. Choose a different worker count
while configuring, for example:

```powershell
cmake --preset win-x64-clangcl-debug-tidy -D SANDBOX_CLANG_TIDY_JOBS=4
cmake --build --preset win-x64-clangcl-debug-tidy
```

Set `SANDBOX_CLANG_TIDY_JOBS=0` to restore the automatic worker count.

## Enabled checks

- `modernize-use-nullptr`
- `modernize-use-override`
- `readability-redundant-control-flow`
- `readability-redundant-string-init`
- `performance-unnecessary-value-param`
- `performance-move-const-arg`
