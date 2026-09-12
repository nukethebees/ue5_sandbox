(load-script "benchmark-fleet.scm")

(define battle-entities (benchmark-fleet-entities 128 16))

(level
  (id 'benchmark-fleet-09)
  (title "Benchmark Fleet 09 — 256 Capitals")
  (description "256 capital ships across two opposing fleets; up to 1792 entities after all fighter wings deploy.")

  (teams
    (team 'blue)
    (team 'red))

  (camera
    (look-at 'blue-capital-0 'red-capital-0)
    (distance 520000)
    (offset-direction -1 -1 0.7))

  (apply entities battle-entities))

