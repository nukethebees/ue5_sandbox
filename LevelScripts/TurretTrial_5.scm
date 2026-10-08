(load-script "Libraries/turret-trials.scm")

(turret-trial 5
  "Push through a staggered turret corridor without becoming boxed in."
  '(0 -74000 1000)
  '((-3000 4000 0)
    (3000 7000 1500)
    (-3000 10000 -1500)
    (3000 13000 1500)
    (0 16000 -1500)))
