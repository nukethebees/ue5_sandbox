(load-script "Libraries/turret-trials.scm")

(turret-trial 6
  "Choose which of two mutually supporting turret clusters to attack first."
  '(0 -73000 1000)
  '((-7000 7500 0)
    (-4000 9500 2000)
    (-7000 11500 -2000)
    (7000 7500 0)
    (4000 9500 -2000)
    (7000 11500 2000)))
