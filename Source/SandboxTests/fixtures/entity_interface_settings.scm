(settings-layers
  (load-script "Settings/base.scm")
  (settings
    :tick (tick :time-scale 100)
    :collision-grid (collision-grid :level-size '(2000000 2000000 2000000))))
