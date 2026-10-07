(level
  :id 'turret-trial-1
  :title "Turret Trial 1"
  :description "Destroy two widely separated turrets one at a time."

  :unlock (list
    (level-completed 'turret-trial-0))

  :teams '(blue
    red)

  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :heroes '(player)
    :must-survive '(player))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -72000 1000)
      :rotation '(0 90 0))

    (entity :id 'turret-0 :archetype 'static-turret :team 'red
      :position '(-6000 12000 0)
      :rotation '(0 -90 0))

    (entity :id 'turret-1 :archetype 'static-turret :team 'red
      :position '(6000 12000 0)
      :rotation '(0 -90 0))))
