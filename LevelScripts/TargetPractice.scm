(level
  (id 'target-practice)
  (title "Target Practice")
  (description "A short introductory mission against a single enemy turret.")

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'kill-enemies)
    (kill-count 1)
    (heroes 'player)
    (must-survive 'player)
    (required-kills 'target-turret))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 -15000 1000)
      (rotation 0 90 0))

    (entity 'target-turret 'static-turret 'red
      (position 0 15000 0)
      (rotation 0 -90 0))))
