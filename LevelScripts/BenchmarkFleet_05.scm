(load-script "benchmark-fleet.scm")

(define battle-entities (benchmark-fleet-entities 48 8))

(level
  (id 'benchmark-fleet-05)
  (title "Benchmark Fleet 05 — 96 Capitals")
  (description "96 capital ships across two opposing fleets; up to 672 entities after all fighter wings deploy.")

  (teams
    (team 'blue)
    (team 'red))

  (camera
    (look-at 'blue-capital-0 'red-capital-0)
    (distance 520000)
    (offset-direction -1 -1 0.7))

  (apply entities battle-entities))

