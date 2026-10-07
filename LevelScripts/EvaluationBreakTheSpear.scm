(level
  :id 'evaluation-break-the-spear
  :title "Evaluation 01: Break the Spear"
  :description "Destroy the enemy flagship while keeping the task group alive for 150 seconds."

  :unlock (list
    (level-completed 'counteroffensive-fleet-termination))

  :teams '(blue
    red)

  :player 'player

  :mission (mission
    :mode 'survive-time
    :time-limit 150
    :heroes '(player blue-capital-0 blue-capital-1)
    :must-survive '(player blue-capital-0 blue-capital-1)
    :required-kills '(red-flagship))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -95000 8000)
      :rotation '(0 90 0))

    (entity :id 'blue-capital-0 :archetype 'capital-ship :team 'blue
      :position '(-35000 -50000 -4000)
      :rotation '(0 90 0))

    (entity :id 'blue-capital-1 :archetype 'capital-ship :team 'blue
      :position '(35000 -50000 4000)
      :rotation '(0 90 0))

    (entity :id 'blue-turret-0 :archetype 'static-turret :team 'blue
      :position '(-85000 -20000 0)
      :rotation '(0 90 0))

    (entity :id 'blue-turret-1 :archetype 'static-turret :team 'blue
      :position '(85000 -20000 0)
      :rotation '(0 90 0))

    (entity :id 'red-flagship :archetype 'capital-ship :team 'red
      :position '(0 115000 5000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-0 :archetype 'capital-ship :team 'red
      :position '(-70000 95000 -5000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-1 :archetype 'capital-ship :team 'red
      :position '(70000 95000 -5000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-2 :archetype 'capital-ship :team 'red
      :position '(0 165000 -5000)
      :rotation '(0 -90 0)
      :spawn-at 75)))
