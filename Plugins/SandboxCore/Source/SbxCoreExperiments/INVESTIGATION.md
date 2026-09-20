# Single-allocation SoA investigation archive

The Unreal/Catch2 benchmark harness used for the 8–9 September 2026 allocator and column-spacing investigation has been retired with the repository's Unreal Low Level Tests. Its committed CSV/PNG results remain historical, machine-specific evidence only:

- [Unreal mimalloc layouts](results/unreal-mimalloc-layouts.csv)
- [Native reserve matrix](results/native-reserve-matrix.csv)
- [Initial manual timings](results/initial-manual-timings.csv)

Current correctness coverage and repeatable native performance work use the generated native-SoA GTests and Google Benchmark workflows:

```powershell
cmake --workflow --preset native-soa
cmake --workflow --preset native-soa-reserve-matrix
```

The experimental Unreal module remains intentionally unchanged; this archive does not support production adoption or recreate the retired Unreal measurements.
