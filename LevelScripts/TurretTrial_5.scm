(level
  (id 'turret-trial-5)
  (title "Turret Trial 5")
  (description "Push through a staggered turret corridor without becoming boxed in.")

  (unlock
    (level-completed 'turret-trial-4))

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
      (position 0 -74000 1000)
      (rotation 0 90 0))

    (entity 'turret-0 'static-turret 'red
      (position -3000 4000 0)
      (rotation 0 -90 0))

    (entity 'turret-1 'static-turret 'red
      (position 3000 7000 1500)
      (rotation 0 -90 0))

    (entity 'turret-2 'static-turret 'red
      (position -3000 10000 -1500)
      (rotation 0 -90 0))

    (entity 'turret-3 'static-turret 'red
      (position 3000 13000 1500)
      (rotation 0 -90 0))

    (entity 'turret-4 'static-turret 'red
      (position 0 16000 -1500)
      (rotation 0 -90 0))))
