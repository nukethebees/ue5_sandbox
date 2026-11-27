(export level-definition)

(define turret-trial (module-ref (import-module "Libraries/turret-trials.scm") 'turret-trial))

(define level-definition
  (turret-trial 0
    "Destroy a single isolated turret and survive."
    '(0 -75000 1000)
    '((0 15000 0))
    :par-time 15))
