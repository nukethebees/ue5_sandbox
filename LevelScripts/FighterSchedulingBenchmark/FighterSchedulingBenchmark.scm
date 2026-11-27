(export level-definition)

(define benchmark-fleet-level (module-ref (import-module "Libraries/benchmark-fleet.scm") 'benchmark-fleet-level))

(define level-definition
  (benchmark-fleet-level 'fighter-scheduling-benchmark
    "Fighter Scheduling Benchmark"
    "Two dense capital fleets for saturating the configured fighter population cap."
    256 16 :front-separation 80000))
