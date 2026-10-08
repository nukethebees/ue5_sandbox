(level
  :id 'turret-trial-7
  :title "Turret Trial 7"
  :description "Break an eight-turret ring before the 120-second limit expires."

  :unlock (list
    (level-completed 'turret-trial-6))

  :player 'player

  :mission (mission
    :mode 'kill-enemies-within-time
    :time-limit 120
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
      '((0 4000 1000)
        (4243 5757 -1000)
        (6000 10000 1000)
        (4243 14243 -1000)
        (0 16000 1000)
        (-4243 14243 -1000)
        (-6000 10000 1000)
        (-4243 5757 -1000)))))
