(export level-definition)

(define benchmark-fleet-level (module-ref (import-module "Libraries/benchmark-fleet.scm") 'benchmark-fleet-level))

(define level-definition
  (benchmark-fleet-level 'benchmark-fleet-05
    "Benchmark Fleet 05 — 96 Capitals"
    "96 capital ships across two opposing fleets; up to 672 entities after all fighter wings deploy."
    48 8))
