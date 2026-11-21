# Layout Planner

Build and open the game's Lispb schema from the repository root:

```powershell
cmake --workflow --preset layout-planner
.\out\build\win-x64-clangcl-debug\tools\layout_planner\app\layout-planner.exe --project lispb/project.lispb --target sandbox-code
```

Select a declaration, edit its source, and click **Apply**. Read errors in **Diagnostics**. **Save** writes applied edits. Use **Save as** for a new project. Inspect field offsets, padding, and storage in the analysis views. For an ABI comparison, load both target profiles. Variant edits stay in the session.
