(load-script "Libraries/benchmark-fleet.scm")

(benchmark-fleet-level 'benchmark-fleet-04
  "Benchmark Fleet 04 — 64 Capitals"
  "64 capital ships across two opposing fleets; up to 448 entities after all fighter wings deploy."
  32 8)
