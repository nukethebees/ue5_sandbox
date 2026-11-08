(load-script "Libraries/benchmark-fleet.scm")

(benchmark-fleet-level 'benchmark-fleet-03
  "Benchmark Fleet 03 — 48 Capitals"
  "48 capital ships across two opposing fleets; up to 336 entities after all fighter wings deploy."
  24 6)
