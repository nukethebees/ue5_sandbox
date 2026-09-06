(level
  (id 'defence-last-anchorage)
  (title "Defence 04: Last Anchorage")
  (description "Hold the fleet anchorage against three carriers and a delayed reserve formation.")

  (unlock
    (level-completed 'defence-dual-custody))

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'survive-time)
    (time-limit 150)
    (must-survive 'player 'blue-capital-0 'blue-capital-1))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 -95000 8000)
      (rotation 0 90 0))

    (entity 'blue-capital-0 'capital-ship 'blue
      (position -30000 -50000 -4000)
      (rotation 0 90 0))

    (entity 'blue-capital-1 'capital-ship 'blue
      (position 30000 -50000 4000)
      (rotation 0 90 0))

    (entity 'blue-picket-0 'static-turret 'blue
      (position -45000 -38000 -2500)
      (rotation 0 90 0))

    (entity 'blue-picket-1 'static-turret 'blue
      (position -30000 -36000 2500)
      (rotation 0 90 0))

    (entity 'blue-picket-2 'static-turret 'blue
      (position -15000 -38000 -2500)
      (rotation 0 90 0))

    (entity 'blue-picket-3 'static-turret 'blue
      (position 15000 -38000 2500)
      (rotation 0 90 0))

    (entity 'blue-picket-4 'static-turret 'blue
      (position 30000 -36000 -2500)
      (rotation 0 90 0))

    (entity 'blue-picket-5 'static-turret 'blue
      (position 45000 -38000 2500)
      (rotation 0 90 0))

    (entity 'red-capital-0 'capital-ship 'red
      (position -55000 70000 6000)
      (rotation 0 -90 0))

    (entity 'red-capital-1 'capital-ship 'red
      (position 0 70000 0)
      (rotation 0 -90 0))

    (entity 'red-capital-2 'capital-ship 'red
      (position 55000 70000 -6000)
      (rotation 0 -90 0))

    (entity 'red-capital-3 'capital-ship 'red
      (position -25000 150000 8000)
      (rotation 0 -90 0)
      (spawn-at 75))))
