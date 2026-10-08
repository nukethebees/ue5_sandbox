(load-script "Libraries/turret-trials.scm")

(turret-trial 8
  "Clear two interlocking turret layers within 105 seconds."
  '(0 -75200 1000)
  '((-4000 7000 -2500)
    (4000 7000 -2500)
    (0 10000 -2500)
    (-4000 13000 -2500)
    (4000 13000 -2500)
    (0 7000 2500)
    (-4000 10000 2500)
    (4000 10000 2500)
    (0 13000 2500)
    (0 16000 2500))
  :time-limit 105)
