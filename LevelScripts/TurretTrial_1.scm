(level
  :id 'turret-trial-1
  :title "Turret Trial 1"
  :description "Destroy two widely separated turrets one at a time."

  :unlock (list
    (level-completed 'turret-trial-0))

  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :heroes '(player)
    :must-survive '(player))

  :entities (cons
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -72000 1000)
      :rotation '(0 90 0))

    (map
      (lambda (position)
        (entity :archetype 'static-turret :team 'red
          :position position
          :rotation '(0 -90 0)))
      '((-6000 12000 0)
        (6000 12000 0)))))
