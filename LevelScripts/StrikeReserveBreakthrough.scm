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

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -95000 8000)
      :rotation '(0 90 0))

    (entity :id 'blue-capital-0 :archetype 'capital-ship :team 'blue
      :position '(-30000 -50000 -4000)
      :rotation '(0 90 0))

    (entity :id 'blue-capital-1 :archetype 'capital-ship :team 'blue
      :position '(30000 -50000 4000)
      :rotation '(0 90 0))

    (entity :id 'red-capital-0 :archetype 'capital-ship :team 'red
      :position '(-30000 70000 4000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-1 :archetype 'capital-ship :team 'red
      :position '(30000 70000 -4000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-2 :archetype 'capital-ship :team 'red
      :position '(0 150000 8000)
      :rotation '(0 -90 0)
      :spawn-at 60)))
