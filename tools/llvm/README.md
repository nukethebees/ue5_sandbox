# Build the project LLVM tools

Maintainer operation. Use the pinned LLVM source revision `3c3e1eeb0ae7030396f01080b4ce520b181406a8`. Place the `llvm-project` checkout beside the existing LLVM installation selected by `LLVM_ROOT` or PATH. Run from the repository root with the Visual Studio Common7/Tools path:

```powershell
./tools/llvm/build.ps1 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools' -Install
```

For checker-only changes, pass `-TidyOnly -Install` against an existing full build. The script runs the checker suites before installation. Built tools target the build machine's CPU. Set `LLVM_ROOT` before configuring consumer builds. [Run clang-tidy](../../docs/clang-tidy.md) · [Checker sources and tests](clang_tidy/).
