(level
  (id 'dev-three-second-failure)
  (title "Dev: Three-Second Failure")
  (description "Development-only mission that guarantees a timeout after three seconds.")

  (teams
    (team 'blue))

  (player 'player)

  (mission
    (mode 'kill-enemies-within-time)
    (time-limit 3)
    (kill-count 1)
    (heroes 'player)
    (must-survive 'player))

  (entities
    (entity 'player 'player-fighter 'blue
      (position 0 0 0)
      (rotation 0 0 0))))
