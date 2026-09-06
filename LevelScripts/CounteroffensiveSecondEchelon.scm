(level
  (id 'counteroffensive-second-echelon)
  (title "Counteroffensive 02: Second Echelon")
  (description "Eliminate the forward carrier group and the reserve ship entering behind it.")

  (unlock
    (level-completed 'counteroffensive-decapitation))

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'kill-enemies-within-time)
    (time-limit 240)
    (kill-count 4)
    (heroes 'player 'blue-capital-0 'blue-capital-1 'blue-capital-2)
    (must-survive 'player)
    (required-kills 'red-capital-0 'red-capital-1 'red-capital-2))

  (mission-events
    (mission-event (at 60)
      (add-required-kills 'red-capital-3)))

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
      (position -45000 70000 6000)
      (rotation 0 -90 0))

    (entity 'red-capital-1 'capital-ship 'red
      (position 0 70000 0)
      (rotation 0 -90 0))

    (entity 'red-capital-2 'capital-ship 'red
      (position 45000 70000 -6000)
      (rotation 0 -90 0))

    (entity 'red-capital-3 'capital-ship 'red
      (position 0 150000 8000)
      (rotation 0 -90 0)
      (spawn-at 60))))
