(level
  :id 'turret-trial-3
  :title "Turret Trial 3"
  :description "Attack a vertical stack of turrets and use all three dimensions."

  :unlock (list
    (level-completed 'turret-trial-2))

  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :heroes '(player)
    :must-survive '(player))

  :entities (cons
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -71000 1000)
      :rotation '(0 90 0))

    (map
      (lambda (index)
        (entity :archetype 'static-turret :team 'red
          :position (list 0 10000 (+ -4000 (* index 4000)))
          :rotation '(0 -90 0)))
      '(0 1 2))))
