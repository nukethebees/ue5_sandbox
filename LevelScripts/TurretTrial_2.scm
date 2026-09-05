(level
  (id 'turret-trial-2)
  (title "Turret Trial 2")
  (description "Break a shallow defensive line before its fields of fire overlap.")

  (unlock
    (level-completed 'turret-trial-1))

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
      (position -5000 12000 0)
      (rotation 0 -90 0))

    (entity 'turret-1 'static-turret 'red
      (position 0 12000 0)
      (rotation 0 -90 0))

    (entity 'turret-2 'static-turret 'red
      (position 5000 12000 0)
      (rotation 0 -90 0))))
