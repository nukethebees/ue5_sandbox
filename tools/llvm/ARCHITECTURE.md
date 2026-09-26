# Static clang-tidy experiment

Recommendation: **switch to statically linked IOJ checks**, under the proposed optional,
machine-level tool model. Keep the DLL baseline for comparison until a separate cleanup pass.
Base: `tooling/custom-clang-tidy-v1` at `ac972d6289`.

## Build evidence

LLVM revision: `688a1498b3ce9011ee58c214086e3cd408e86f5e`, 24.0.0git. Windows x64,
MSVC 14.50.35717 (compiler 19.50.35737), SDK 10.0.22621.0, Ninja, Release, assertions and PDBs on.
`build.ps1` records the exact configure arguments; LLVM's exception handling and RTTI remain off.

The complete integration patch appends this at the absolute end of
`clang-tools-extra/clang-tidy/CMakeLists.txt`:

```cmake
if(IOJ_TIDY_SOURCE_DIR)
  add_subdirectory("${IOJ_TIDY_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/ioj-tidy")
endif()
```

Set `IOJ_TIDY_SOURCE_DIR` to this worktree's `cmake/clang_tidy/static`. Both `clangTidy` and
`clang-tidy` already exist there. Our OBJECT library links privately to `clangTidy` for its
dependencies, and `target_link_libraries(clang-tidy PRIVATE ioj-tidy-checks)` puts both IOJ
objects directly on the executable link line. Registration works without an anchor, changes to
`ALL_CLANG_TIDY_CHECKS`, or changes to `ClangTidyForceLinker.h`. Checker C++ is unchanged.

`llvm_update_compile_flags(ioj-tidy-checks)` applies LLVM's own EH/RTTI policy. This also avoids
MSVC C4530 from the C++23 standard-library headers when exceptions are disabled.

The experiment configured all of these OFF:

```text
LLVM_ENABLE_PLUGINS
LLVM_EXPORT_SYMBOLS_FOR_PLUGINS
CLANG_PLUGIN_SUPPORT
```

`llvm-readobj --coff-exports clang-tidy.exe` reported no exports. A fresh component install
contained just `clang-tidy.exe`, its PDB, `run-clang-tidy`, and `clang-tidy-diff.py`: no import
library, DLL, or LLVM/Clang development packages. The two previous install-CMake fixes are still
present locally for the baseline, but their executable-import-library behavior is inactive.
They are not needed for static IOJ checks. Other plugin consumers may still need them.

Observed validation:

- `-checks=-*,ioj-loop-condition-call -list-checks` lists the check without `-load`.
- Existing semantic tests pass: three test methods, including 14 expression/suppression cases,
  nested simulation policy, benchmark exclusion, and inherited native checks.
- The preserved DLL host/module also pass those same tests and load successfully through the
  optional-plugin runner, emitting both IOJ and built-in diagnostics.
- `clang-tidy-simulation`: 119 translation units, zero diagnostics; inspected log has no `-load`.
- All 10 native workflow tests pass, covering both linkage modes, paths with spaces, exit status,
  scoped dependencies/filters, and standard-check fallback without development packages.
- Unsetting `IOJ_TIDY_SOURCE_DIR` configures successfully; building `clang-tidy` then only relinks
  an ordinary executable without IOJ checks. The real runner still emits a built-in
  `modernize-use-nullptr` diagnostic under the nested simulation config.
- Touching `LoopConditionCallCheck.cpp` recompiles **one object and relinks clang-tidy**, with no
  LLVM library rebuild or configure step: **11.67 seconds** including developer-shell setup.
  The immediately following build reports `ninja: no work to do`.

The separately fixed static-analyzer node-lifetime bug is unrelated to either linkage model.
Its patch remains necessary at this pinned LLVM revision. Native runtime and MSVC Unreal
integration are unchanged; their preceding validation was not rerun for this tooling experiment.

## Comparison

| Concern | DLL baseline | Static IOJ checks |
| --- | --- | --- |
| LLVM source changes for linkage | Two install fixes | One conditional subdirectory hook |
| LLVM configuration | Plugin support and executable exports | Neither required |
| Installation | Headers, package exports, executable import library | Normal tool binaries/resource headers |
| Checker implementation | Existing V1 check and registry initializer | Identical C++ |
| Checker build | MODULE plus manual host configuration/runtime matching | OBJECT plus LLVM's compile policy |
| Game CMake | Finds development packages, builds DLL, adds dependencies and `-load` | Probes installed checks; invokes existing runner |
| Windows fragility | Export coverage, import library, registry identity, runtime ABI | Ordinary executable link; no plugin ABI boundary |
| Incremental edits | Recompile checker and relink small worktree DLL | Recompile checker and relink larger LLVM executable |
| Worktree behavior | Checker naturally follows each branch | Installed checker can be absent or stale for a branch |
| LLVM upgrades | Refresh export/install patches and DLL compatibility | Reapply end-of-file hook and compile/test against new LLVM APIs |
| Reproducibility | Matching host installation plus per-worktree plugin build | Pinned LLVM, repository-owned checker, explicit build/install script |

Static coupling is acceptable for a small, stable, optional policy check. Normal work can proceed
without IOJ checks; agents must not build LLVM to satisfy unrelated tasks. The presence probe
does **not** prove that the installed check matches a branch's implementation. Checker authors
must explicitly build and test their branch against the LLVM build executable before installing.
Concurrent checker development should use separate LLVM build/install directories and coordinate
changes to the shared installation. This costs more disk/link time than the DLL approach.
If IOJ checks become mandatory gates that must follow every branch automatically, reconsider
that tradeoff: a global static executable alone cannot provide that guarantee.

## Deferred DLL cleanup

The comparison switch defaults off; the baseline implementation remains on this branch and the
original branch. Do not load its DLL into the static executable, which already registers IOJ.
To rebuild a pure DLL host, unset `IOJ_TIDY_SOURCE_DIR` and enable the baseline plugin/export
options in a separate LLVM build/install directory. The old machine script alone does not clear
the static hook's cached source path.
After accepting the static design, a separate pass can:

1. Remove IOJ's requirement for `LLVM_EXPORT_SYMBOLS_FOR_PLUGINS=ON` and `CLANG_PLUGIN_SUPPORT=ON`
   from the old machine build script. No IOJ-specific executable exports are needed.
2. Revert the `AddLLVM.cmake` change setting `ENABLE_EXPORTS` before installation and the
   `AddClang.cmake` change adding `ARCHIVE DESTINATION` to `add_clang_tool`, if no other plugin uses them.
3. Stop installing/distributing the executable import library `clang-tidy.lib` for IOJ.
4. Remove `cmake/clang_tidy/plugin/CMakeLists.txt`: MODULE construction, `find_package(LLVM/Clang)`,
   derived package directories, imported-executable linkage, and host Release/runtime/RTTI/assertion matching.
5. Remove `IOJ_CLANG_TIDY_USE_DLL`, conditional DLL target dependencies, and plugin-path arguments
   from `cmake/clang_tidy/CMakeLists.txt`.
6. Remove the optional `Plugin` parameter and `-load` forwarding from `run_clang_tidy.ps1`, the
   semantic test harness's `--plugin` argument, and DLL-only workflow-test cases.
7. Rename the shared checker source directory from `plugin` to `checks`, updating static CMake,
   test paths, and formatting scope. Keep the existing check and nested policy unchanged.

Keep `LLVM_ROOT`, the clang-cl/MSVC ABI settings, existing tidy workflows/filters/generator
dependencies, and the independent analyzer lifetime fix. Those are not DLL-specific.
