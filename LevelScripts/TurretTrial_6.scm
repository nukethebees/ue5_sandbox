(level
  (id 'turret-trial-6)
  (title "Turret Trial 6")
  (description "Choose which of two mutually supporting turret clusters to attack first.")

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
      (position 0 -73000 1000)
      (rotation 0 90 0))

    (entity 'turret-0 'static-turret 'red
      (position -7000 7500 0)
      (rotation 0 -90 0))

    (entity 'turret-1 'static-turret 'red
      (position -4000 9500 2000)
      (rotation 0 -90 0))

    (entity 'turret-2 'static-turret 'red
      (position -7000 11500 -2000)
      (rotation 0 -90 0))

    (entity 'turret-3 'static-turret 'red
      (position 7000 7500 0)
      (rotation 0 -90 0))

    (entity 'turret-4 'static-turret 'red
      (position 4000 9500 -2000)
      (rotation 0 -90 0))

    (entity 'turret-5 'static-turret 'red
      (position 7000 11500 2000)
      (rotation 0 -90 0))))
