(level
  :id 'counteroffensive-decapitation
  :title "Counteroffensive 01: Decapitation"
  :description "Destroy an outnumbering enemy capital formation before its operating window closes."

  :unlock (list
    (level-completed 'defence-last-anchorage))

  :player 'player

  :mission (mission
    :mode 'kill-enemies-within-time
    :time-limit 210
    :kill-count 3
    :heroes '(player blue-capital-0 blue-capital-1)
    :must-survive '(player)
    :required-kills '(red-capital-0 red-capital-1 red-capital-2))

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
      '((-25000 -50000 -4000)
        (25000 -50000 4000)))

    (map
      (lambda (index)
        (entity :archetype 'capital-ship :team 'red
          :id (string->symbol (format #f "red-capital-~A" index))
          :position (list (+ -45000 (* index 45000)) 70000 (+ 6000 (* index -6000)))
          :rotation '(0 -90 0)))
      '(0 1 2))))
