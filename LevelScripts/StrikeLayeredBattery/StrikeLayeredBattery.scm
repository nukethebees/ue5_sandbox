(export level-definition)

(define logic-library (import-module "Libraries/logic.scm"))
(define destroy-any (module-ref logic-library 'destroy-any))
(define win-race (module-ref logic-library 'win-race))
(define entities-at (module-ref (import-module "Libraries/formations.scm") 'entities-at))

(define level-definition
  (level :settings (module-ref (import-module "Settings/level-defaults.scm") 'game-settings)
    :id 'strike-layered-battery
    :title "Strike 02: Layered Battery"
    :description "Clear two mutually supporting turret clusters distributed across multiple elevations."

    :unlock (list
      (level-completed 'strike-range-clearance))

    :player 'player

    :logic (logic
      (win-race 'blue 'red
      (entity-kills-reached '(player) 8)
      (destroy-any '(player))))

    :entities (cons
      (entity :id 'player :archetype 'player-fighter :team 'blue
        :position '(0 -75000 1000)
        :rotation '(0 90 0))

      (entities-at 'static-turret 'red '(0 -90 0)
        '((-12000 10000 -2500)
          (-8000 6000 2500)
          (-4000 10000 2500)
          (-8000 14000 -2500)
          (4000 10000 -2500)
          (8000 6000 -2500)
          (12000 10000 2500)
          (8000 14000 2500))))))
