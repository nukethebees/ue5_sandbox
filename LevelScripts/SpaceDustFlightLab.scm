(level
  (id 'space-dust-flight-lab)
  (title "Space Dust Flight Lab")
  (description "A quiet player-flight lab for tuning velocity-driven space dust. A friendly capital ship provides an optional depth-occlusion reference.")

  (teams
    (team 'blue))

  (player 'player)

  (mission
    (mode 'survive-time)
    (time-limit 3600)
    (heroes 'player)
    (must-survive 'player))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 0 0)
      (rotation 0 0 0))

    (entity 'occlusion-reference 'capital-ship 'blue
      (position 50000 0 0)
      (rotation 0 180 0))))
