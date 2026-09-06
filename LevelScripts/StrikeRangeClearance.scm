(level
  (id 'strike-range-clearance)
  (title "Strike 01: Range Clearance")
  (description "Eliminate an isolated three-turret battery and return to operational control.")

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
      (position -6000 15000 -1500)
      (rotation 0 -90 0))

    (entity 'turret-1 'static-turret 'red
      (position 0 15000 1500)
      (rotation 0 -90 0))

    (entity 'turret-2 'static-turret 'red
      (position 6000 15000 -1500)
      (rotation 0 -90 0))))
