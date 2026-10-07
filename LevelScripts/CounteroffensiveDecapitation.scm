(level
  :id 'counteroffensive-decapitation
  :title "Counteroffensive 01: Decapitation"
  :description "Destroy an outnumbering enemy capital formation before its operating window closes."

  :unlock (list
    (level-completed 'defence-last-anchorage))

  :teams '(blue
    red)

  :player 'player

  :mission (mission
    :mode 'kill-enemies-within-time
    :time-limit 210
    :kill-count 3
    :heroes '(player blue-capital-0 blue-capital-1)
    :must-survive '(player)
    :required-kills '(red-capital-0 red-capital-1 red-capital-2))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -95000 8000)
      :rotation '(0 90 0))

    (entity :id 'blue-capital-0 :archetype 'capital-ship :team 'blue
      :position '(-25000 -50000 -4000)
      :rotation '(0 90 0))

    (entity :id 'blue-capital-1 :archetype 'capital-ship :team 'blue
      :position '(25000 -50000 4000)
      :rotation '(0 90 0))

    (entity :id 'red-capital-0 :archetype 'capital-ship :team 'red
      :position '(-45000 70000 6000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-1 :archetype 'capital-ship :team 'red
      :position '(0 70000 0)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-2 :archetype 'capital-ship :team 'red
      :position '(45000 70000 -6000)
      :rotation '(0 -90 0))))
