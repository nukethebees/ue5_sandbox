(load-script "Libraries/objectives.scm")
(level
  :id 'dev-three-second-failure
  :title "Dev: Three-Second Failure"
  :description "Development-only mission that guarantees a timeout after three seconds."

  :player 'player

  :mission (decisive-mission 'blue
    (entity-kills-reached '(player) 1)
    (any-of
      (destroy-any '(player))
      (time-elapsed 3)))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 0 0))))
