(load-script "Libraries/benchmark-fleet.scm")

(benchmark-fleet-level 'benchmark-fleet-06
  "Benchmark Fleet 06 — 128 Capitals"
  "128 capital ships across two opposing fleets; up to 896 entities after all fighter wings deploy."
  64 8)
