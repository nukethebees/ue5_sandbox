(export level-definition)

(define benchmark-fleet-level (module-ref (import-module "Libraries/benchmark-fleet.scm") 'benchmark-fleet-level))

(define level-definition
  (benchmark-fleet-level 'benchmark-fleet-02
    "Benchmark Fleet 02 — 32 Capitals"
    "32 capital ships across two opposing fleets; up to 224 entities after all fighter wings deploy."
    16 4))
