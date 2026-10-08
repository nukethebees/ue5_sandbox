(level
  :id 'turret-trial-6
  :title "Turret Trial 6"
  :description "Choose which of two mutually supporting turret clusters to attack first."

  :unlock (list
    (level-completed 'turret-trial-5))

  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :heroes '(player)
    :must-survive '(player))

  :entities (cons
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -73000 1000)
      :rotation '(0 90 0))

    (map
      (lambda (position)
        (entity :archetype 'static-turret :team 'red
          :position position
          :rotation '(0 -90 0)))
      '((-7000 7500 0)
        (-4000 9500 2000)
        (-7000 11500 -2000)
        (7000 7500 0)
        (4000 9500 -2000)
        (7000 11500 2000)))))
