(load-script "benchmark-fleet.scm")

(define battle-entities (benchmark-fleet-entities 8 4))

(level
  (id 'benchmark-fleet-01)
  (title "Benchmark Fleet 01 — 16 Capitals")
  (description "16 capital ships across two opposing fleets; up to 112 entities after all fighter wings deploy.")

  (teams
    (team 'blue)
    (team 'red))

  (camera
    (look-at 'blue-capital-0 'red-capital-0)
    (distance 520000)
    (offset-direction -1 -1 0.7))

  (apply entities battle-entities))

