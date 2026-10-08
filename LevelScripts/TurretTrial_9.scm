(level
  :id 'turret-trial-9
  :title "Turret Trial 9"
  :description "Destroy a dense double ring of twelve turrets within 90 seconds."

  :unlock (list
    (level-completed 'turret-trial-8))

  :player 'player

  :mission (mission
    :mode 'kill-enemies-within-time
    :time-limit 90
    :heroes '(player)
    :must-survive '(player))

  :entities (cons
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -74000 1000)
      :rotation '(0 90 0))

    (map
      (lambda (position)
        (entity :archetype 'static-turret :team 'red
          :position position
          :rotation '(0 -90 0)))
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
        (-2250 6103 2200)))))
