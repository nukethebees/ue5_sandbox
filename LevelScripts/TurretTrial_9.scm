(load-script "Libraries/turret-trials.scm")

(turret-trial 9
  "Destroy a dense double ring of twelve turrets within 90 seconds."
  '(0 -74000 1000)
  '((0 5500 -2200)
    (3897 7750 -2200)
    (3897 12250 -2200)
    (0 14500 -2200)
    (-3897 12250 -2200)
    (-3897 7750 -2200)
    (2250 6103 2200)
    (4500 10000 2200)
    (2250 13897 2200)
    (-2250 13897 2200)
    (-4500 10000 2200)
    (-2250 6103 2200))
  :time-limit 90)
