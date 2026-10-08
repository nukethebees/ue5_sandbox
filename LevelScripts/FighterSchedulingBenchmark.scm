(load-script "Libraries/benchmark-fleet.scm")

(benchmark-fleet-level 'fighter-scheduling-benchmark
  "Fighter Scheduling Benchmark"
  "Two dense capital fleets for saturating the configured fighter population cap."
  256 16 :front-separation 80000)
