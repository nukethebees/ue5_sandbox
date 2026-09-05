(level
  (id 'turret-trial-1)
  (title "Turret Trial 1")
  (description "Destroy two widely separated turrets one at a time.")

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
      (position 0 -72000 1000)
      (rotation 0 90 0))

    (entity 'turret-0 'static-turret 'red
      (position -6000 12000 0)
      (rotation 0 -90 0))

    (entity 'turret-1 'static-turret 'red
      (position 6000 12000 0)
      (rotation 0 -90 0))))
