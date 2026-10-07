(level
  :id 'turret-trial-3
  :title "Turret Trial 3"
  :description "Attack a vertical stack of turrets and use all three dimensions."

  :unlock (list
    (level-completed 'turret-trial-2))

  :teams '(blue
    red)

  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :heroes '(player)
    :must-survive '(player))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -71000 1000)
      :rotation '(0 90 0))

    (entity :id 'turret-0 :archetype 'static-turret :team 'red
      :position '(0 10000 -4000)
      :rotation '(0 -90 0))

    (entity :id 'turret-1 :archetype 'static-turret :team 'red
      :position '(0 10000 0)
      :rotation '(0 -90 0))

    (entity :id 'turret-2 :archetype 'static-turret :team 'red
      :position '(0 10000 4000)
      :rotation '(0 -90 0))))
