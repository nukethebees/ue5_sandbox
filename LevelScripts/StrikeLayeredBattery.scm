(level
  (id 'strike-layered-battery)
  (title "Strike 02: Layered Battery")
  (description "Clear two mutually supporting turret clusters distributed across multiple elevations.")

  (unlock
    (level-completed 'strike-range-clearance))

  (teams
    (team 'blue)
    (team 'red))

  (player 'player)

  (mission
    (mode 'kill-enemies)
    (heroes 'player)
    (must-survive 'player))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 -75000 1000)
      (rotation 0 90 0))

    (entity 'turret-0 'static-turret 'red
      (position -12000 10000 -2500)
      (rotation 0 -90 0))

    (entity 'turret-1 'static-turret 'red
      (position -8000 6000 2500)
      (rotation 0 -90 0))

    (entity 'turret-2 'static-turret 'red
      (position -4000 10000 2500)
      (rotation 0 -90 0))

    (entity 'turret-3 'static-turret 'red
      (position -8000 14000 -2500)
      (rotation 0 -90 0))

    (entity 'turret-4 'static-turret 'red
      (position 4000 10000 -2500)
      (rotation 0 -90 0))

    (entity 'turret-5 'static-turret 'red
      (position 8000 6000 -2500)
      (rotation 0 -90 0))

    (entity 'turret-6 'static-turret 'red
      (position 12000 10000 2500)
      (rotation 0 -90 0))

    (entity 'turret-7 'static-turret 'red
      (position 8000 14000 2500)
      (rotation 0 -90 0))))
