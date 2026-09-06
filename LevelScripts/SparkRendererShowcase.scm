(level
  (id 'spark-renderer-showcase)
  (title "Spark Renderer Showcase")
  (description "An autonomous close-range crossfire staged to showcase analytic laser-impact sparks.")

  (teams
    (team 'blue)
    (team 'red))

  (camera
    (look-at 'blue-capital-left 'blue-capital-right
             'red-capital-left 'red-capital-right)
    (distance 150000)
    (offset-direction -1 -1 0.7))

  (entities
    (entity 'blue-capital-left 'capital-ship 'blue
      (position -35000 -65000 -6000)
      (rotation 0 90 0))

    (entity 'blue-capital-right 'capital-ship 'blue
      (position 35000 -65000 6000)
      (rotation 0 90 0))

    (entity 'blue-turret-0 'static-turret 'blue
      (position -45000 -25000 -3000)
      (rotation 0 90 0))

    (entity 'blue-turret-1 'static-turret 'blue
      (position -15000 -25000 3000)
      (rotation 0 90 0))

    (entity 'blue-turret-2 'static-turret 'blue
      (position 15000 -25000 -3000)
      (rotation 0 90 0))

    (entity 'blue-turret-3 'static-turret 'blue
      (position 45000 -25000 3000)
      (rotation 0 90 0))

    (entity 'red-turret-0 'static-turret 'red
      (position -45000 25000 3000)
      (rotation 0 -90 0))

    (entity 'red-turret-1 'static-turret 'red
      (position -15000 25000 -3000)
      (rotation 0 -90 0))

    (entity 'red-turret-2 'static-turret 'red
      (position 15000 25000 3000)
      (rotation 0 -90 0))

    (entity 'red-turret-3 'static-turret 'red
      (position 45000 25000 -3000)
      (rotation 0 -90 0))

    (entity 'red-capital-left 'capital-ship 'red
      (position -35000 65000 6000)
      (rotation 0 -90 0))

    (entity 'red-capital-right 'capital-ship 'red
      (position 35000 65000 -6000)
      (rotation 0 -90 0))))
