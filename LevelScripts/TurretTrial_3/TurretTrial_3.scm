(export level-definition)

(define positions-on-line (module-ref (import-module "Libraries/formations.scm") 'positions-on-line))
(define turret-trial (module-ref (import-module "Libraries/turret-trials.scm") 'turret-trial))

(define level-definition
  (turret-trial 3
    "Attack a vertical stack of turrets and use all three dimensions."
    '(0 -71000 1000)
    (positions-on-line 3 '(0 10000 -4000) '(0 0 4000))))
