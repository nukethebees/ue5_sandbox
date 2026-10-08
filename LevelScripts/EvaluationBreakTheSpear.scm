(load-script "Libraries/formations.scm")

(level
  :id 'evaluation-break-the-spear
  :title "Evaluation 01: Break the Spear"
  :description "Destroy the enemy flagship while keeping the task group alive for 150 seconds."

  :unlock (list
    (level-completed 'counteroffensive-fleet-termination))

  :player 'player

  :mission (mission
    :mode 'survive-time
    :time-limit 150
    :heroes '(player blue-capital-0 blue-capital-1)
    :must-survive '(player blue-capital-0 blue-capital-1)
    :required-kills '(red-flagship))

  :entities (append
    (list
      (entity :id 'player :archetype 'player-fighter :team 'blue
        :position '(0 -95000 8000)
        :rotation '(0 90 0)))

    (named-capitals 'blue '(0 90 0)
      '((blue-capital-0 (-35000 -50000 -4000))
        (blue-capital-1 (35000 -50000 4000))))

    (entities-at 'static-turret 'blue '(0 90 0)
      '((-85000 -20000 0)
        (85000 -20000 0)))

    (list
      (entity :id 'red-flagship :archetype 'capital-ship :team 'red
        :position '(0 115000 5000)
        :rotation '(0 -90 0)))

    (entities-at 'capital-ship 'red '(0 -90 0)
      '((-70000 95000 -5000)
        (70000 95000 -5000)))

    (list
      (entity :archetype 'capital-ship :team 'red
        :position '(0 165000 -5000)
        :rotation '(0 -90 0)
        :spawn-at 75))))
