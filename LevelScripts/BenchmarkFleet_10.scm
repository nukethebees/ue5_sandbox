(load-script "Libraries/benchmark-fleet.scm")

(benchmark-fleet-level 'benchmark-fleet-10
  "Benchmark Fleet 10 — 320 Capitals"
  "320 capital ships across two opposing fleets; up to 2240 entities after all fighter wings deploy."
  160 16)
