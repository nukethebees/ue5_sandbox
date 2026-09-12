(load-script "benchmark-fleet.scm")

(define battle-entities (benchmark-fleet-entities 104 13))

(level
  (id 'benchmark-fleet-08)
  (title "Benchmark Fleet 08 — 208 Capitals")
  (description "208 capital ships across two opposing fleets; up to 1456 entities after all fighter wings deploy.")

  (teams
    (team 'blue)
    (team 'red))

  (camera
    (look-at 'blue-capital-0 'red-capital-0)
    (distance 520000)
    (offset-direction -1 -1 0.7))

  (apply entities battle-entities))

