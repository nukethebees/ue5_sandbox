(level
  :id 'turret-trial-4
  :title "Turret Trial 4"
  :description "Dismantle a diamond formation with overlapping coverage."

  :unlock (list
    (level-completed 'turret-trial-3))

  :player 'player

  :mission (mission
    :mode 'kill-enemies
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
      '((-4000 10000 0)
        (4000 10000 0)
        (0 6000 2000)
        (0 14000 -2000)))))
