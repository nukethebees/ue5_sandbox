(level
  (id 'defence-screen-duty)
  (title "Defence 01: Screen Duty")
  (description "Keep the assigned capital ship operational until the enemy attack window closes.")

  (unlock
    (level-completed 'strike-reserve-breakthrough))

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'survive-time)
    (time-limit 75)
    (must-survive 'player 'blue-capital))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 -95000 8000)
      (rotation 0 90 0))

    (entity 'blue-capital 'capital-ship 'blue
      (position 0 -50000 0)
      (rotation 0 90 0))

    (entity 'red-capital 'capital-ship 'red
      (position 0 70000 0)
      (rotation 0 -90 0))))
