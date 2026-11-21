# Native clang-tidy

For a requested audit, configure the compilation database and select a subsystem:

```powershell
cmake --preset win-x64-clangcl-debug-tidy
coj tidy --scope simulation --build-dir out/build/win-x64-clangcl-debug/clang-tidy
```

Include consumers when shared headers change. Use `--scope native` for the full audit. Standard checks come from [native policy](../native/.clang-tidy). Simulation adds [its own checks](../native/simulation/.clang-tidy). [Build or update the LLVM checkers](../tools/llvm/README.md).
