(load-script "Libraries/benchmark-fleet.scm")

(benchmark-fleet-level 'benchmark-fleet-01
  "Benchmark Fleet 01 — 16 Capitals"
  "16 capital ships across two opposing fleets; up to 112 entities after all fighter wings deploy."
  8 4)
