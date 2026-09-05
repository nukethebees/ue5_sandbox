(level
  (id 'turret-trial-8)
  (title "Turret Trial 8")
  (description "Clear two interlocking turret layers within 105 seconds.")

  (unlock
    (level-completed 'turret-trial-7))

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'kill-enemies-within-time)
    (time-limit 105)
    (heroes 'player)
    (must-survive 'player))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 -75200 1000)
      (rotation 0 90 0))

    (entity 'turret-0 'static-turret 'red
      (position -4000 7000 -2500)
      (rotation 0 -90 0))

    (entity 'turret-1 'static-turret 'red
      (position 4000 7000 -2500)
      (rotation 0 -90 0))

    (entity 'turret-2 'static-turret 'red
      (position 0 10000 -2500)
      (rotation 0 -90 0))

    (entity 'turret-3 'static-turret 'red
      (position -4000 13000 -2500)
      (rotation 0 -90 0))

    (entity 'turret-4 'static-turret 'red
      (position 4000 13000 -2500)
      (rotation 0 -90 0))

    (entity 'turret-5 'static-turret 'red
      (position 0 7000 2500)
      (rotation 0 -90 0))

    (entity 'turret-6 'static-turret 'red
      (position -4000 10000 2500)
      (rotation 0 -90 0))

    (entity 'turret-7 'static-turret 'red
      (position 4000 10000 2500)
      (rotation 0 -90 0))

    (entity 'turret-8 'static-turret 'red
      (position 0 13000 2500)
      (rotation 0 -90 0))

    (entity 'turret-9 'static-turret 'red
      (position 0 16000 2500)
      (rotation 0 -90 0))))
