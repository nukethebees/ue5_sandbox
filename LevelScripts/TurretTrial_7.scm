(level
  (id 'turret-trial-7)
  (title "Turret Trial 7")
  (description "Break an eight-turret ring before the 120-second limit expires.")

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'kill-enemies-within-time)
    (time-limit 120)
    (heroes 'player)
    (must-survive 'player))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 -74000 1000)
      (rotation 0 90 0))

    (entity 'turret-0 'static-turret 'red
      (position 0 4000 1000)
      (rotation 0 -90 0))

    (entity 'turret-1 'static-turret 'red
      (position 4243 5757 -1000)
      (rotation 0 -90 0))

    (entity 'turret-2 'static-turret 'red
      (position 6000 10000 1000)
      (rotation 0 -90 0))

    (entity 'turret-3 'static-turret 'red
      (position 4243 14243 -1000)
      (rotation 0 -90 0))

    (entity 'turret-4 'static-turret 'red
      (position 0 16000 1000)
      (rotation 0 -90 0))

    (entity 'turret-5 'static-turret 'red
      (position -4243 14243 -1000)
      (rotation 0 -90 0))

    (entity 'turret-6 'static-turret 'red
      (position -6000 10000 1000)
      (rotation 0 -90 0))

    (entity 'turret-7 'static-turret 'red
      (position -4243 5757 -1000)
      (rotation 0 -90 0))))
