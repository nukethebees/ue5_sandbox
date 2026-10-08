(level
  :id 'strike-reserve-breakthrough
  :title "Strike 04: Reserve Breakthrough"
  :description "Destroy two enemy capital ships, then neutralise the reserve carrier entering the battlespace."

  :unlock (list
    (level-completed 'strike-carrier-intercept))

  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :kill-count 3
    :heroes '(player blue-capital-0 blue-capital-1)
    :must-survive '(player)
    :required-kills '(red-capital-0 red-capital-1))

  :mission-events (list
    (mission-event :at 60
      :add-required-kills '(red-capital-2)))

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
      '((-30000 -50000 -4000)
        (30000 -50000 4000)))

    (map
      (lambda (index position)
        (entity :archetype 'capital-ship :team 'red
          :id (string->symbol (format #f "red-capital-~A" index))
          :position position
          :rotation '(0 -90 0)))
      '(0 1)
      '((-30000 70000 4000)
        (30000 70000 -4000)))

    (list
      (entity :id 'red-capital-2 :archetype 'capital-ship :team 'red
        :position '(0 150000 8000)
        :rotation '(0 -90 0)
        :spawn-at 60))))
