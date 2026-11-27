(export level-definition)

(define logic-library (import-module "Libraries/logic.scm"))
(define destroy-all (module-ref logic-library 'destroy-all))
(define destroy-any (module-ref logic-library 'destroy-any))
(define level-delay (module-ref logic-library 'level-delay))
(define win-race (module-ref logic-library 'win-race))
(define named-capitals (module-ref (import-module "Libraries/formations.scm") 'named-capitals))

(define level-definition
  (let ((reserve-id 'red-capital-3)
        (reserve-time 60))
    (level :settings (module-ref (import-module "Settings/level-defaults.scm") 'game-settings)
      :id 'counteroffensive-second-echelon
      :title "Counteroffensive 02: Second Echelon"
      :description "Eliminate the forward carrier group and the reserve ship entering behind it."

      :unlock (list
        (level-completed 'counteroffensive-decapitation))

      :player 'player

      :logic (logic
      (win-race 'blue 'red
        (all-of
          (entity-kills-reached
            '(player blue-capital-0 blue-capital-1 blue-capital-2)
            4)
          (destroy-all
            (append
              '(red-capital-0 red-capital-1 red-capital-2)
              (list reserve-id))))
        (any-of
          (destroy-any '(player))
          (level-delay 240))))

      :entities (append
        (list
          (entity :id 'player :archetype 'player-fighter :team 'blue
            :position '(0 -95000 8000)
            :rotation '(0 90 0)))

        (named-capitals 'blue '(0 90 0)
          '((blue-capital-0 (-40000 -55000 -5000))
            (blue-capital-1 (0 -55000 0))
            (blue-capital-2 (40000 -55000 5000))))

        (named-capitals 'red '(0 -90 0)
          '((red-capital-0 (-45000 70000 6000))
            (red-capital-1 (0 70000 0))
            (red-capital-2 (45000 70000 -6000))))

        (list
          (entity :id reserve-id :archetype 'capital-ship :team 'red
            :position '(0 150000 8000)
            :rotation '(0 -90 0)
            :spawn-at reserve-time))))))
