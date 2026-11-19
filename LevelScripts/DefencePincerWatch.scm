(export mission)

(define logic-library (import-module "Libraries/logic.scm"))
(define destroy-any (module-ref logic-library 'destroy-any))
(define level-delay (module-ref logic-library 'level-delay))
(define win-race (module-ref logic-library 'win-race))
(define entities-at (module-ref (import-module "Libraries/formations.scm") 'entities-at))

(define mission
  (level :settings (module-ref (import-module "Settings/base.scm") 'game-settings)
    :id 'defence-pincer-watch
    :title "Defence 02: Pincer Watch"
    :description "Protect a capital ship and its forward pickets from a two-axis carrier attack."

    :unlock (list
      (level-completed 'defence-screen-duty))

    :player 'player

    :logic (logic
      (win-race 'blue 'red
      (level-delay 90)
      (destroy-any '(player blue-capital))))

    :entities (append
      (list
        (entity :id 'player :archetype 'player-fighter :team 'blue
          :position '(0 -95000 8000)
          :rotation '(0 90 0)))

      (list
        (entity :id 'blue-capital :archetype 'capital-ship :team 'blue
          :position '(0 -50000 0)
          :rotation '(0 90 0)))

      (entities-at 'static-turret 'blue '(0 90 0)
        '((-3000 -40000 -2000)
          (3000 -40000 2000)))

      (entities-at 'capital-ship 'red '(0 -90 0)
        '((-40000 70000 5000)
          (40000 70000 -5000))))))
