(load-script "benchmark-fleet.scm")

(define battle-entities (benchmark-fleet-entities 160 16))

(level
  (id 'benchmark-fleet-10)
  (title "Benchmark Fleet 10 — 320 Capitals")
  (description "320 capital ships across two opposing fleets; up to 2240 entities after all fighter wings deploy.")

  (teams
    (team 'blue)
    (team 'red))

  (camera
    (look-at 'blue-capital-0 'red-capital-0)
    (distance 520000)
    (offset-direction -1 -1 0.7))

  (apply entities battle-entities))

