(export level-definition)

(define turret-trial (module-ref (import-module "Libraries/turret-trials.scm") 'turret-trial))

(define level-definition
  (turret-trial 4
    "Dismantle a diamond formation with overlapping coverage."
    '(0 -74000 1000)
    '((-4000 10000 0)
      (4000 10000 0)
      (0 6000 2000)
      (0 14000 -2000))))
