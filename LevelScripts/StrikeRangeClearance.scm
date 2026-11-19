(export mission)

(define logic-library (import-module "Libraries/logic.scm"))
(define destroy-any (module-ref logic-library 'destroy-any))
(define win-race (module-ref logic-library 'win-race))
(define entities-at (module-ref (import-module "Libraries/formations.scm") 'entities-at))

(define mission
  (level :settings (module-ref (import-module "Settings/base.scm") 'game-settings)
    :id 'strike-range-clearance
    :title "Strike 01: Range Clearance"
    :description "Eliminate an isolated three-turret battery and return to operational control."

    :player 'player

    :logic (logic
      (win-race 'blue 'red
      (entity-kills-reached '(player) 3)
      (destroy-any '(player))))

    :entities (cons
      (entity :id 'player :archetype 'player-fighter :team 'blue
        :position '(0 -75000 1000)
        :rotation '(0 90 0))

      (entities-at 'static-turret 'red '(0 -90 0)
        '((-6000 15000 -1500)
          (0 15000 1500)
          (6000 15000 -1500))))))
