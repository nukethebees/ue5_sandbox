(level
  (id 'evaluation-lone-screen)
  (title "Evaluation 03: Lone Screen")
  (description "Survive alone through repeated enemy fighter launches for two minutes.")

  (unlock
    (level-completed 'evaluation-distributed-defence))

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'survive-time)
    (time-limit 120)
    (heroes 'player)
    (must-survive 'player))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 -120000 0)
      (rotation 0 90 0))

    (entity 'red-carrier-0 'capital-ship 'red
      (position 0 130000 0)
      (rotation 0 -90 0))

    (entity 'red-carrier-1 'capital-ship 'red
      (position -85000 175000 6000)
      (rotation 0 -90 0)
      (spawn-at 35))

    (entity 'red-carrier-2 'capital-ship 'red
      (position 85000 175000 -6000)
      (rotation 0 -90 0)
      (spawn-at 75))))
