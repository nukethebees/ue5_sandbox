(level
  :id 'turret-trial-2
  :title "Turret Trial 2"
  :description "Break a shallow defensive line before its fields of fire overlap."

  :unlock (list
    (level-completed 'turret-trial-1))

  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :heroes '(player)
    :must-survive '(player))

  :entities (cons
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -75000 1000)
      :rotation '(0 90 0))

    (map
      (lambda (index)
        (entity :archetype 'static-turret :team 'red
          :position (list (+ -5000 (* index 5000)) 12000 0)
          :rotation '(0 -90 0)))
      '(0 1 2))))
