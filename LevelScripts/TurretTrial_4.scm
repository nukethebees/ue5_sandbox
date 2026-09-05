(level
  (id 'turret-trial-4)
  (title "Turret Trial 4")
  (description "Dismantle a diamond formation with overlapping coverage.")

  (unlock
    (level-completed 'turret-trial-3))

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
      (position 0 -74000 1000)
      (rotation 0 90 0))

    (entity 'turret-0 'static-turret 'red
      (position -4000 10000 0)
      (rotation 0 -90 0))

    (entity 'turret-1 'static-turret 'red
      (position 4000 10000 0)
      (rotation 0 -90 0))

    (entity 'turret-2 'static-turret 'red
      (position 0 6000 2000)
      (rotation 0 -90 0))

    (entity 'turret-3 'static-turret 'red
      (position 0 14000 -2000)
      (rotation 0 -90 0))))
