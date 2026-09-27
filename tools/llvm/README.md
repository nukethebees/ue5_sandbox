# Project LLVM toolchain

This is an explicit machine-tooling operation. Normal game workflows use installed tools and run
standard clang-tidy checks when IOJ checks are absent. They never build or repair LLVM.

## Requirements

- LLVM 24.0.0git checkout at `e0b3e4c82911376fcb2dfcbc4a3dd3f4f5891aba`.
- Windows x64, MSVC 14.50.35717, Windows SDK 10.0.22621.0, CMake, Ninja, Git, Python and PowerShell.
- An x64 developer shell initialized with
  `VsDevCmd.bat -arch=x64 -host_arch=x64 -vcvars_ver=14.50.35717 -winsdk=10.0.22621.0`.

## Build and install

Prepare the pinned checkout explicitly, then run from this repository in the developer shell:

```powershell
./tools/llvm/build.ps1 -LLVMSource D:/llvm/llvm-project -BuildDir D:/llvm/build -LLVMRoot D:/llvm/install -Install
```

These paths are examples. `-LLVMSource` and `-BuildDir` are required; `-LLVMRoot` defaults to the
`LLVM_ROOT` environment variable. `-Jobs` defaults to 24. Use a fresh build directory when changing
compiler or configuration. Omit `-Install` to build and verify without changing installed tools.
Coordinate costly builds through jobserver, and installation with users of that installation.

The script verifies the revision before changing source, applies each owned patch independently,
accepts already-applied patches, and rejects unrelated source edits. It does not fetch or update LLVM.
When updating the pin, first reverse the owned patches, update the clean checkout explicitly, and
review whether each patch is still needed.

The configuration is Ninja, X86, Release, assertions/PDBs on, EH/RTTI off, with `clang` and
`clang-tools-extra`. The newly built Clang builds `compiler-rt` builtins and Windows x64 ASAN
through `LLVM_ENABLE_RUNTIMES`, including upstream's supporting sanitizer libraries.
Profiling, XRay, libFuzzer and ORC runtimes are disabled.
Runtime builds explicitly use the pinned MSVC linker and SDK manifest tool.
`LLVM_ENABLE_PLUGINS`, `LLVM_EXPORT_SYMBOLS_FOR_PLUGINS` and
`CLANG_PLUGIN_SUPPORT` are off. Examples, benchmarks and extra-tools tests are disabled.

The build targets are `clang`, `clang-format`, `clang-tidy`, `clang-scan-deps`, `llvm-ar`, `llvm-nm`, `llvm-readobj`
and `runtimes`, including their dependencies. Before installation, the script checks IOJ
registration and runs the semantic and nested-policy tests against the newly built executable.
A failed check blocks installation.

`-Install` establishes a project `LLVM_ROOT` from scratch: those tools, the `clang-cl`/`llvm-lib`
aliases, Clang resource headers, `run-clang-tidy`, supporting tidy scripts, enabled PDBs and
compiler-rt runtimes. Native CMake uses this root's `llvm-lib` for archives and `llvm-nm`/`llvm-readobj`
for mimalloc symbol generation and audits. An explicit root never borrows missing tools from PATH;
an empty root retains normal tool discovery. Non-unity C++ builds use the same root's
`clang-scan-deps` for CMake dependency scanning. LLVM development packages are unnecessary.

The install verifies its tools and resource headers, plus these native ASAN dependencies under
`lib/clang/24/lib/windows`: `clang_rt.asan_dynamic-x86_64.dll`,
`clang_rt.asan_dynamic-x86_64.lib` and `clang_rt.asan_dynamic_runtime_thunk-x86_64.lib`.
Run `cmake --workflow --preset win-x64-clangcl-debug-asan` with `LLVM_ROOT` set to the installation
to build and test the existing native ASAN configuration. It still requires the pinned MSVC/SDK.

## Project patches and static checks

- [0001-add-ioj-clang-tidy-subdirectory.patch](patches/0001-add-ioj-clang-tidy-subdirectory.patch)
  adds the conditional `IOJ_TIDY_SOURCE_DIR` subdirectory at the end of upstream clang-tidy CMake.
- [0002-fix-analyzer-control-dependency-lifetime.patch](patches/0002-fix-analyzer-control-dependency-lifetime.patch)
  fixes an independent analyzer node-lifetime bug and includes its regression test. The pinned
  revision still needs it; analyzer checks remain enabled.

`clang_tidy/CMakeLists.txt` builds the sources under `clang_tidy/lib` as an OBJECT library, using
LLVM's compilation policy and `clangTidy` dependency. Linking that target into `clang-tidy` puts
objects directly into the executable. Registration needs no linker anchor or registry changes.
Without `IOJ_TIDY_SOURCE_DIR`, the patched upstream project builds ordinary clang-tidy.

## Checker development

Rebuild the same tree after editing a checker. An implementation edit recompiles its object and
relinks clang-tidy. The installed executable is a machine-level snapshot; game workflows do not
rebuild it automatically when a worktree changes.

```powershell
cmake --build D:/llvm/build --target clang-tidy --parallel 24
D:/llvm/build/bin/clang-tidy.exe '-checks=-*,ioj-loop-condition-call' -list-checks
python tools/llvm/clang_tidy/tests/test_loop_condition_call.py --clang-tidy D:/llvm/build/bin/clang-tidy.exe --source-dir .
```

Run the semantic tests before installation, then configure and run `clang-tidy-simulation` against
the updated `LLVM_ROOT`. See [project tidy workflows](../../docs/clang-tidy.md) for scope selection.
