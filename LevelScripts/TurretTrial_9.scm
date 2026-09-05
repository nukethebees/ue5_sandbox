(level
  (id 'turret-trial-9)
  (title "Turret Trial 9")
  (description "Destroy a dense double ring of twelve turrets within 90 seconds.")

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'kill-enemies-within-time)
    (time-limit 90)
    (heroes 'player)
    (must-survive 'player))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 -74000 1000)
      (rotation 0 90 0))

    (entity 'turret-0 'static-turret 'red
      (position 0 5500 -2200)
      (rotation 0 -90 0))

    (entity 'turret-1 'static-turret 'red
      (position 3897 7750 -2200)
      (rotation 0 -90 0))

    (entity 'turret-2 'static-turret 'red
      (position 3897 12250 -2200)
      (rotation 0 -90 0))

    (entity 'turret-3 'static-turret 'red
      (position 0 14500 -2200)
      (rotation 0 -90 0))

    (entity 'turret-4 'static-turret 'red
      (position -3897 12250 -2200)
      (rotation 0 -90 0))

    (entity 'turret-5 'static-turret 'red
      (position -3897 7750 -2200)
      (rotation 0 -90 0))

    (entity 'turret-6 'static-turret 'red
      (position 2250 6103 2200)
      (rotation 0 -90 0))

    (entity 'turret-7 'static-turret 'red
      (position 4500 10000 2200)
      (rotation 0 -90 0))

    (entity 'turret-8 'static-turret 'red
      (position 2250 13897 2200)
      (rotation 0 -90 0))

    (entity 'turret-9 'static-turret 'red
      (position -2250 13897 2200)
      (rotation 0 -90 0))

    (entity 'turret-10 'static-turret 'red
      (position -4500 10000 2200)
      (rotation 0 -90 0))

    (entity 'turret-11 'static-turret 'red
      (position -2250 6103 2200)
      (rotation 0 -90 0))))
