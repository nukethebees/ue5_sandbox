(level
  (id 'counteroffensive-decapitation)
  (title "Counteroffensive 01: Decapitation")
  (description "Destroy an outnumbering enemy capital formation before its operating window closes.")

  (unlock
    (level-completed 'defence-last-anchorage))

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'kill-enemies-within-time)
    (time-limit 210)
    (kill-count 3)
    (heroes 'player 'blue-capital-0 'blue-capital-1)
    (must-survive 'player)
    (required-kills 'red-capital-0 'red-capital-1 'red-capital-2))

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

    (entity 'red-capital-0 'capital-ship 'red
      (position -45000 70000 6000)
      (rotation 0 -90 0))

    (entity 'red-capital-1 'capital-ship 'red
      (position 0 70000 0)
      (rotation 0 -90 0))

    (entity 'red-capital-2 'capital-ship 'red
      (position 45000 70000 -6000)
      (rotation 0 -90 0))))
