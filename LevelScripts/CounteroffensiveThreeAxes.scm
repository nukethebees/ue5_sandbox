(level
  (id 'counteroffensive-three-axes)
  (title "Counteroffensive 03: Three Axes")
  (description "Destroy a dispersed capital force approaching across three axes and its delayed reserve.")

  (unlock
    (level-completed 'counteroffensive-second-echelon))

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'kill-enemies-within-time)
    (time-limit 270)
    (kill-count 5)
    (heroes 'player 'blue-capital-0 'blue-capital-1 'blue-capital-2)
    (must-survive 'player)
    (required-kills 'red-capital-0 'red-capital-1 'red-capital-2 'red-capital-3))

  (mission-events
    (mission-event (at 75)
      (add-required-kills 'red-capital-4)))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 -95000 8000)
      (rotation 0 90 0))

    (entity 'blue-capital-0 'capital-ship 'blue
      (position -40000 -55000 -5000)
      (rotation 0 90 0))

    (entity 'blue-capital-1 'capital-ship 'blue
      (position 0 -55000 0)
      (rotation 0 90 0))

    (entity 'blue-capital-2 'capital-ship 'blue
      (position 40000 -55000 5000)
      (rotation 0 90 0))

    (entity 'red-capital-0 'capital-ship 'red
      (position -90000 50000 8000)
      (rotation 0 -70 0))

    (entity 'red-capital-1 'capital-ship 'red
      (position 90000 50000 -8000)
      (rotation 0 -110 0))

    (entity 'red-capital-2 'capital-ship 'red
      (position -30000 100000 -4000)
      (rotation 0 -90 0))

    (entity 'red-capital-3 'capital-ship 'red
      (position 30000 100000 4000)
      (rotation 0 -90 0))

    (entity 'red-capital-4 'capital-ship 'red
      (position 0 170000 0)
      (rotation 0 -90 0)
      (spawn-at 75))))
