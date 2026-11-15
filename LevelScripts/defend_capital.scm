(load-script "Libraries/logic.scm")

(level-with (load-script "defend_capital/editor-config.scm")
  :settings (settings-layers
    (load-script "Settings/base.scm")
    (settings
      :collision-grid (collision-grid
        :level-size '(2000000 2000000 500000)
        :cell-size '(5000 5000 20000))))

  :logic (logic
    (win-race 'blue 'red
      (all-of
        (level-delay 60)
        (destroy-all '(capitalship-red2)))
      (any-of
        (destroy-any '(capitalship-blue))
        (level-delay 60)))))
