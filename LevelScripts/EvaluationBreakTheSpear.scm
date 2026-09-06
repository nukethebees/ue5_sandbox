(level
  (id 'evaluation-break-the-spear)
  (title "Evaluation 01: Break the Spear")
  (description "Destroy the enemy flagship while keeping the task group alive for 150 seconds.")

  (unlock
    (level-completed 'counteroffensive-fleet-termination))

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'survive-time)
    (time-limit 150)
    (heroes 'player 'blue-capital-0 'blue-capital-1)
    (must-survive 'player 'blue-capital-0 'blue-capital-1)
    (required-kills 'red-flagship))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 -95000 8000)
      (rotation 0 90 0))

    (entity 'blue-capital-0 'capital-ship 'blue
      (position -35000 -50000 -4000)
      (rotation 0 90 0))

    (entity 'blue-capital-1 'capital-ship 'blue
      (position 35000 -50000 4000)
      (rotation 0 90 0))

    (entity 'blue-turret-0 'static-turret 'blue
      (position -85000 -20000 0)
      (rotation 0 90 0))

    (entity 'blue-turret-1 'static-turret 'blue
      (position 85000 -20000 0)
      (rotation 0 90 0))

    (entity 'red-flagship 'capital-ship 'red
      (position 0 115000 5000)
      (rotation 0 -90 0))

    (entity 'red-capital-0 'capital-ship 'red
      (position -70000 95000 -5000)
      (rotation 0 -90 0))

    (entity 'red-capital-1 'capital-ship 'red
      (position 70000 95000 -5000)
      (rotation 0 -90 0))

    (entity 'red-capital-2 'capital-ship 'red
      (position 0 165000 -5000)
      (rotation 0 -90 0)
      (spawn-at 75))))
