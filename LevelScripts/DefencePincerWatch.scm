(level
  (id 'defence-pincer-watch)
  (title "Defence 02: Pincer Watch")
  (description "Protect a capital ship and its forward pickets from a two-axis carrier attack.")

  (unlock
    (level-completed 'defence-screen-duty))

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'survive-time)
    (time-limit 90)
    (must-survive 'player 'blue-capital))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 -95000 8000)
      (rotation 0 90 0))

    (entity 'blue-capital 'capital-ship 'blue
      (position 0 -50000 0)
      (rotation 0 90 0))

    (entity 'blue-picket-0 'static-turret 'blue
      (position -3000 -40000 -2000)
      (rotation 0 90 0))

    (entity 'blue-picket-1 'static-turret 'blue
      (position 3000 -40000 2000)
      (rotation 0 90 0))

    (entity 'red-capital-0 'capital-ship 'red
      (position -40000 70000 5000)
      (rotation 0 -90 0))

    (entity 'red-capital-1 'capital-ship 'red
      (position 40000 70000 -5000)
      (rotation 0 -90 0))))
