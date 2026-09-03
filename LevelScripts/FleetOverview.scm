(level
  (title "Fleet Overview")
  (description "A playerless battle viewed from an authored camera between two flagships.")

  (teams
    (team 'blue)
    (team 'red))

  (camera
    (look-at 'blue-capital 'red-capital)
    (distance 180000)
    (offset-direction -1 -1 0.6))

  (entities
    (entity 'blue-capital 'capital-ship 'blue
      (position -70000 0 0)
      (rotation 0 0 0))

    (entity 'blue-turret 'static-turret 'blue
      (position -55000 -25000 0)
      (rotation 0 0 0))

    (entity 'red-capital 'capital-ship 'red
      (position 70000 0 0)
      (rotation 0 180 0))

    (entity 'red-turret 'static-turret 'red
      (position 55000 25000 0)
      (rotation 0 180 0))))
