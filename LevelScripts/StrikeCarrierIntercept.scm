(level
  (id 'strike-carrier-intercept)
  (title "Strike 03: Carrier Intercept")
  (description "Support a fleet carrier and destroy an enemy capital ship protected by a forward battery.")

  (unlock
    (level-completed 'strike-layered-battery))

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'kill-enemies)
    (kill-count 1)
    (heroes 'player 'blue-capital)
    (must-survive 'player)
    (required-kills 'red-capital))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 -85000 6000)
      (rotation 0 90 0))

    (entity 'blue-capital 'capital-ship 'blue
      (position 0 -40000 0)
      (rotation 0 90 0))

    (entity 'red-capital 'capital-ship 'red
      (position 0 60000 0)
      (rotation 0 -90 0))

    (entity 'red-turret-0 'static-turret 'red
      (position -7000 48000 -2500)
      (rotation 0 -90 0))

    (entity 'red-turret-1 'static-turret 'red
      (position 7000 48000 2500)
      (rotation 0 -90 0))))
