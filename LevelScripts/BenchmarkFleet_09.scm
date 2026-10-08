(load-script "Libraries/benchmark-fleet.scm")

(benchmark-fleet-level 'benchmark-fleet-09
  "Benchmark Fleet 09 — 256 Capitals"
  "256 capital ships across two opposing fleets; up to 1792 entities after all fighter wings deploy."
  128 16)
