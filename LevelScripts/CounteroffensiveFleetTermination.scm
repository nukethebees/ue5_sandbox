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
      '(0 1 2 3)
      '((-60000 -55000 -6000)
        (-20000 -55000 2000)
        (20000 -55000 -2000)
        (60000 -55000 6000)))

    (map
      (lambda (index position)
        (entity :archetype 'capital-ship :team 'red
          :id (string->symbol (format #f "red-capital-~A" index))
          :position position
          :rotation '(0 -90 0)))
      '(0 1 2 3)
      '((-75000 75000 8000)
        (-25000 75000 -3000)
        (25000 75000 3000)
        (75000 75000 -8000)))

    (list
      (entity :id 'red-capital-4 :archetype 'capital-ship :team 'red
        :position '(-35000 160000 8000)
        :rotation '(0 -90 0)
        :spawn-at 75))

    (list
      (entity :id 'red-capital-5 :archetype 'capital-ship :team 'red
        :position '(35000 180000 -8000)
        :rotation '(0 -90 0)
        :spawn-at 150))))
