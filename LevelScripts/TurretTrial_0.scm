(level
  (id 'turret-trial-0)
  (title "Turret Trial 0")
  (description "Destroy a single isolated turret and survive.")

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'kill-enemies)
    (heroes 'player)
    (must-survive 'player))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 -75000 1000)
      (rotation 0 90 0))

    (entity 'turret-0 'static-turret 'red
      (position 0 15000 0)
      (rotation 0 -90 0))))
