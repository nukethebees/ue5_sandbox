(level
  (id 'turret-trial-3)
  (title "Turret Trial 3")
  (description "Attack a vertical stack of turrets and use all three dimensions.")

  (unlock
    (level-completed 'turret-trial-2))

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
      (position 0 -71000 1000)
      (rotation 0 90 0))

    (entity 'turret-0 'static-turret 'red
      (position 0 10000 -4000)
      (rotation 0 -90 0))

    (entity 'turret-1 'static-turret 'red
      (position 0 10000 0)
      (rotation 0 -90 0))

    (entity 'turret-2 'static-turret 'red
      (position 0 10000 4000)
      (rotation 0 -90 0))))
