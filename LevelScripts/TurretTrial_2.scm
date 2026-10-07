(level
  :id 'turret-trial-2
  :title "Turret Trial 2"
  :description "Break a shallow defensive line before its fields of fire overlap."

  :unlock (list
    (level-completed 'turret-trial-1))

  :teams '(blue
    red)

  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :heroes '(player)
    :must-survive '(player))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -75000 1000)
      :rotation '(0 90 0))

    (entity :id 'turret-0 :archetype 'static-turret :team 'red
      :position '(-5000 12000 0)
      :rotation '(0 -90 0))

    (entity :id 'turret-1 :archetype 'static-turret :team 'red
      :position '(0 12000 0)
      :rotation '(0 -90 0))

    (entity :id 'turret-2 :archetype 'static-turret :team 'red
      :position '(5000 12000 0)
      :rotation '(0 -90 0))))
