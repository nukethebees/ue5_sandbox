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

    (map
      (lambda (index position)
        (entity :archetype 'capital-ship :team 'blue
          :id (string->symbol (format #f "blue-capital-~A" index))
          :position position
          :rotation '(0 90 0)))
      '(0 1)
      '((-35000 -50000 -4000)
        (35000 -50000 4000)))

    (map
      (lambda (position)
        (entity :archetype 'static-turret :team 'blue
          :position position
          :rotation '(0 90 0)))
      '((-85000 -20000 0)
        (85000 -20000 0)))

    (list
      (entity :id 'red-flagship :archetype 'capital-ship :team 'red
        :position '(0 115000 5000)
        :rotation '(0 -90 0)))

    (map
      (lambda (position)
        (entity :archetype 'capital-ship :team 'red
          :position position
          :rotation '(0 -90 0)))
      '((-70000 95000 -5000)
        (70000 95000 -5000)))

    (list
      (entity :archetype 'capital-ship :team 'red
        :position '(0 165000 -5000)
        :rotation '(0 -90 0)
        :spawn-at 75))))
