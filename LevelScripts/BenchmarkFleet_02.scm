(load-script "benchmark-fleet.scm")

(define battle-entities (benchmark-fleet-entities 16 4))

(level
  (id 'benchmark-fleet-02)
  (title "Benchmark Fleet 02 — 32 Capitals")
  (description "32 capital ships across two opposing fleets; up to 224 entities after all fighter wings deploy.")

  (teams
    (team 'blue)
    (team 'red))

  (camera
    (look-at 'blue-capital-0 'red-capital-0)
    (distance 520000)
    (offset-direction -1 -1 0.7))

  (apply entities battle-entities))

