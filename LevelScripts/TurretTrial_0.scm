(level
  :id 'turret-trial-0
  :title "Turret Trial 0"
  :description "Destroy a single isolated turret and survive."
  :par-time 15

  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :heroes '(player)
    :must-survive '(player))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -75000 1000)
      :rotation '(0 90 0))

    (entity :archetype 'static-turret :team 'red
      :position '(0 15000 0)
      :rotation '(0 -90 0))))
