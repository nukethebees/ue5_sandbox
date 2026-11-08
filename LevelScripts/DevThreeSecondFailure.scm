(load-script "Libraries/logic.scm")
(level :settings (load-script "Settings/base.scm")
  :id 'dev-three-second-failure
  :title "Dev: Three-Second Failure"
  :description "Development-only mission that guarantees a timeout after three seconds."

  :player 'player

  :logic (logic
    (win-race 'blue 'red
    (entity-kills-reached '(player) 1)
    (any-of
      (destroy-any '(player))
      (level-delay 3))))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 0 0))))
