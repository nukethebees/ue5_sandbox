(load-script "benchmark-fleet.scm")

(define battle-entities (benchmark-fleet-entities 80 10))

(level
  (id 'benchmark-fleet-07)
  (title "Benchmark Fleet 07 — 160 Capitals")
  (description "160 capital ships across two opposing fleets; up to 1120 entities after all fighter wings deploy.")

  (teams
    (team 'blue)
    (team 'red))

  (camera
    (look-at 'blue-capital-0 'red-capital-0)
    (distance 520000)
    (offset-direction -1 -1 0.7))

  (apply entities battle-entities))

