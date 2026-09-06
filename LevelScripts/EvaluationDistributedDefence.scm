(level
  (id 'evaluation-distributed-defence)
  (title "Evaluation 02: Distributed Defence")
  (description "Hold three separated defence installations against a converging capital-ship attack.")

  (unlock
    (level-completed 'evaluation-break-the-spear))

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'survive-time)
    (time-limit 180)
    (heroes 'player 'blue-installation-west 'blue-installation-centre 'blue-installation-east)
    (must-survive 'player 'blue-installation-west 'blue-installation-centre 'blue-installation-east))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 -105000 7000)
      (rotation 0 90 0))

    (entity 'blue-installation-west 'static-turret 'blue
      (position -110000 -20000 0)
      (rotation 0 90 0))

    (entity 'blue-installation-centre 'static-turret 'blue
      (position 0 0 0)
      (rotation 0 90 0))

    (entity 'blue-installation-east 'static-turret 'blue
      (position 110000 -20000 0)
      (rotation 0 90 0))

    (entity 'blue-capital-0 'capital-ship 'blue
      (position 0 -55000 -6000)
      (rotation 0 90 0))

    (entity 'red-capital-west 'capital-ship 'red
      (position -150000 85000 6000)
      (rotation 0 -90 0))

    (entity 'red-capital-centre 'capital-ship 'red
      (position 0 115000 -6000)
      (rotation 0 -90 0))

    (entity 'red-capital-east 'capital-ship 'red
      (position 150000 85000 6000)
      (rotation 0 -90 0))

    (entity 'red-capital-reserve-west 'capital-ship 'red
      (position -90000 165000 -6000)
      (rotation 0 -90 0)
      (spawn-at 70))

    (entity 'red-capital-reserve-east 'capital-ship 'red
      (position 90000 165000 -6000)
      (rotation 0 -90 0)
      (spawn-at 120))))
