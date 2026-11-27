(export level-definition)

(define turret-trial (module-ref (import-module "Libraries/turret-trials.scm") 'turret-trial))

(define level-definition
  (turret-trial 1
    "Destroy two widely separated turrets one at a time."
    '(0 -72000 1000)
    '((-6000 12000 0)
      (6000 12000 0))))
