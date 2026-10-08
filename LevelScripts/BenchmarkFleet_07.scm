(load-script "Libraries/benchmark-fleet.scm")

(benchmark-fleet-level 'benchmark-fleet-07
  "Benchmark Fleet 07 — 160 Capitals"
  "160 capital ships across two opposing fleets; up to 1120 entities after all fighter wings deploy."
  80 10)
