(load-script "benchmark-fleet.scm")

(define battle-entities (benchmark-fleet-entities 24 6))

(level
  (id 'benchmark-fleet-03)
  (title "Benchmark Fleet 03 — 48 Capitals")
  (description "48 capital ships across two opposing fleets; up to 336 entities after all fighter wings deploy.")

  (teams
    (team 'blue)
    (team 'red))

  (camera
    (look-at 'blue-capital-0 'red-capital-0)
    (distance 520000)
    (offset-direction -1 -1 0.7))

  (apply entities battle-entities))

