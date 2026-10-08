(load-script "Libraries/turret-trials.scm")

(turret-trial 3
  "Attack a vertical stack of turrets and use all three dimensions."
  '(0 -71000 1000)
  (positions-on-line 3 '(0 10000 -4000) '(0 0 4000)))
