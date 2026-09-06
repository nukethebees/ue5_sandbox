(level
  (id 'strike-reserve-breakthrough)
  (title "Strike 04: Reserve Breakthrough")
  (description "Destroy two enemy capital ships, then neutralise the reserve carrier entering the battlespace.")

  (unlock
    (level-completed 'strike-carrier-intercept))

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'kill-enemies)
    (kill-count 3)
    (heroes 'player 'blue-capital-0 'blue-capital-1)
    (must-survive 'player)
    (required-kills 'red-capital-0 'red-capital-1))

  (mission-events
    (mission-event (at 60)
      (add-required-kills 'red-capital-2)))

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
