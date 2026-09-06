(level
  (id 'defence-dual-custody)
  (title "Defence 03: Dual Custody")
  (description "Maintain two capital ships through the initial assault and a reserve carrier arrival.")

  (unlock
    (level-completed 'defence-pincer-watch))

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'survive-time)
    (time-limit 120)
    (must-survive 'player 'blue-capital-0 'blue-capital-1))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 -95000 8000)
      (rotation 0 90 0))

    (entity 'blue-capital-0 'capital-ship 'blue
      (position -25000 -50000 -4000)
      (rotation 0 90 0))

    (entity 'blue-capital-1 'capital-ship 'blue
      (position 25000 -50000 4000)
      (rotation 0 90 0))

    (entity 'blue-picket-0 'static-turret 'blue
      (position -32000 -38000 -2000)
      (rotation 0 90 0))

    (entity 'blue-picket-1 'static-turret 'blue
      (position -18000 -38000 2000)
      (rotation 0 90 0))

    (entity 'blue-picket-2 'static-turret 'blue
      (position 18000 -38000 -2000)
      (rotation 0 90 0))

    (entity 'blue-picket-3 'static-turret 'blue
      (position 32000 -38000 2000)
      (rotation 0 90 0))

    (entity 'red-capital-0 'capital-ship 'red
      (position -30000 70000 4000)
      (rotation 0 -90 0))

    (entity 'red-capital-1 'capital-ship 'red
      (position 30000 70000 -4000)
      (rotation 0 -90 0))

    (entity 'red-capital-2 'capital-ship 'red
      (position 0 150000 8000)
      (rotation 0 -90 0)
      (spawn-at 60))))
