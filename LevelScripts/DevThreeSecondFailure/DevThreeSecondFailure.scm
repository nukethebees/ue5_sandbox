(export level-definition)

(define logic-library (import-module "Libraries/logic.scm"))
(define destroy-any (module-ref logic-library 'destroy-any))
(define level-delay (module-ref logic-library 'level-delay))
(define win-race (module-ref logic-library 'win-race))
(define level-definition
  (level :settings (module-ref (import-module "Settings/level-defaults.scm") 'game-settings)
    :id 'dev-three-second-failure
    :title "Dev: Three-Second Failure"
    :description "Development-only level that guarantees a timeout after three seconds."

    :player 'player

    :logic (logic
      (win-race 'blue 'red
      (entity-kills-reached '(player) 1)
      (any-of
        (destroy-any '(player))
        (level-delay 3))))

    :entities (list
      (entity :id 'player :archetype 'player-fighter :team 'blue
        :position '(0 0 0)))))
