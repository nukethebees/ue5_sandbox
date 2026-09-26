# IOJ clang-tidy build

This is an explicit infrastructure operation. Normal game/tidy workflows use the installed tool,
run standard checks if IOJ checks are absent, and never build or repair LLVM.

Use an existing LLVM checkout at revision `688a1498b3ce9011ee58c214086e3cd408e86f5e` (24.0.0git).
From the game repository, apply the hook once:

```powershell
git -C C:/dev/llvm/llvm-project apply "$PWD/tools/llvm/patches/0001-add-ioj-clang-tidy-subdirectory.patch"
```

That revision also needs the independent analyzer lifetime fix for full native audits:
`cmake/clang_tidy/llvm-patches/control-dependency-node-lifetime.patch`. It is already applied to
the local checkout. Neither patch should be reapplied if already present.

In an x64 Visual Studio developer shell initialized with
`VsDevCmd.bat -arch=x64 -host_arch=x64 -vcvars_ver=14.50.35717 -winsdk=10.0.22621.0`, run:

```powershell
./tools/llvm/build.ps1 -Install
cmake --workflow --preset clang-tidy-simulation
ctest --test-dir out/build/win-x64-clangcl-debug/clang-tidy -L clang-tidy --output-on-failure
```

The script configures the pinned source, builds only `clang-tidy`, verifies IOJ registration,
and optionally installs only that component. `-LLVMSource`, `-BuildDir`, `-LLVMRoot`, and `-Jobs`
override its defaults. Use a fresh build directory if its cached compiler differs from the
selected developer shell. This component install supplements a normal LLVM installation with
clang-cl, clang-format, and Clang's resource headers; it does not install an entire toolchain.

For checker development, omit `-Install` and test the build executable directly:

```powershell
python cmake/clang_tidy/plugin/test_loop_condition_call.py --clang-tidy C:/dev/llvm/build/bin/clang-tidy.exe --source-dir .
```

The installed tool belongs to the machine, not a worktree. Coordinate installation when branches
change checker behavior. See [experiment results and the cleanup list](ARCHITECTURE.md).
