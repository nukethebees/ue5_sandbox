(load-script "benchmark-fleet.scm")

(define battle-entities
  (benchmark-fleet-entities-with-separation 256 16 80000))

(level
  (id 'fighter-scheduling-benchmark)
  (title "Fighter Scheduling Benchmark")
  (description "Two dense capital fleets for saturating the configured fighter population cap.")

  (teams
    (team 'blue)
    (team 'red))

  (camera
    (look-at 'blue-capital-0 'red-capital-0)
    (distance 520000)
    (offset-direction -1 -1 0.7))

  (apply entities battle-entities))
