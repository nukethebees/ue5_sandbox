(load-script "Libraries/turret-trials.scm")

(turret-trial 2
  "Break a shallow defensive line before its fields of fire overlap."
  '(0 -75000 1000)
  (positions-on-line 3 '(-5000 12000 0) '(5000 0 0)))
