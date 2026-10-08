(load-script "Libraries/turret-trials.scm")

(turret-trial 7
  "Break an eight-turret ring before the 120-second limit expires."
  '(0 -74000 1000)
  '((0 4000 1000)
    (4243 5757 -1000)
    (6000 10000 1000)
    (4243 14243 -1000)
    (0 16000 1000)
    (-4243 14243 -1000)
    (-6000 10000 1000)
    (-4243 5757 -1000))
  :time-limit 120)
