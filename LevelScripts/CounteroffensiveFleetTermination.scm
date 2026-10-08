(level
  :id 'counteroffensive-fleet-termination
  :title "Counteroffensive 04: Fleet Termination"
  :description "Destroy the enemy fleet and both reserve carriers before the operation expires."

  :unlock (list
    (level-completed 'counteroffensive-three-axes))


  :player 'player

  :mission (mission
    :mode 'kill-enemies-within-time
    :time-limit 330
    :kill-count 6
    :heroes '(player blue-capital-0 blue-capital-1 blue-capital-2 blue-capital-3)
    :must-survive '(player)
    :required-kills '(red-capital-0 red-capital-1 red-capital-2 red-capital-3))

  :mission-events (list
    (mission-event :at 75
      :add-required-kills '(red-capital-4))
    (mission-event :at 150
      :add-required-kills '(red-capital-5)))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -95000 8000)
      :rotation '(0 90 0))

    (entity :id 'blue-capital-0 :archetype 'capital-ship :team 'blue
      :position '(-60000 -55000 -6000)
      :rotation '(0 90 0))

    (entity :id 'blue-capital-1 :archetype 'capital-ship :team 'blue
      :position '(-20000 -55000 2000)
      :rotation '(0 90 0))

    (entity :id 'blue-capital-2 :archetype 'capital-ship :team 'blue
      :position '(20000 -55000 -2000)
      :rotation '(0 90 0))

    (entity :id 'blue-capital-3 :archetype 'capital-ship :team 'blue
      :position '(60000 -55000 6000)
      :rotation '(0 90 0))

    (entity :id 'red-capital-0 :archetype 'capital-ship :team 'red
      :position '(-75000 75000 8000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-1 :archetype 'capital-ship :team 'red
      :position '(-25000 75000 -3000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-2 :archetype 'capital-ship :team 'red
      :position '(25000 75000 3000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-3 :archetype 'capital-ship :team 'red
      :position '(75000 75000 -8000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-4 :archetype 'capital-ship :team 'red
      :position '(-35000 160000 8000)
      :rotation '(0 -90 0)
      :spawn-at 75)

    (entity :id 'red-capital-5 :archetype 'capital-ship :team 'red
      :position '(35000 180000 -8000)
      :rotation '(0 -90 0)
      :spawn-at 150)))
