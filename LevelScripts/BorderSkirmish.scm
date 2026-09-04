(level
  (id 'border-skirmish)
  (title "Border Skirmish")
  (description "A two-team encounter demonstrating scripted level construction.")

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'kill-enemies)
    (heroes 'player 'blue-capital)
    (must-survive 'blue-capital)
    (required-kills 'red-capital))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 -25000 1000)
      (rotation 0 90 0))

    (entity 'blue-capital 'capital-ship 'blue
      (position -40000 0 0)
      (rotation 0 0 0))

    (entity 'red-capital 'capital-ship 'red
      (position 40000 0 0)
      (rotation 0 180 0))

    (entity 'red-turret 'static-turret 'red
      (position 30000 15000 0)
      (rotation 0 180 0))))
