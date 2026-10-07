(level
  :id 'defence-pincer-watch
  :title "Defence 02: Pincer Watch"
  :description "Protect a capital ship and its forward pickets from a two-axis carrier attack."

  :unlock (list
    (level-completed 'defence-screen-duty))

  :teams '(blue
    red)

  :player 'player

  :mission (mission
    :mode 'survive-time
    :time-limit 90
    :must-survive '(player blue-capital))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -95000 8000)
      :rotation '(0 90 0))

    (entity :id 'blue-capital :archetype 'capital-ship :team 'blue
      :position '(0 -50000 0)
      :rotation '(0 90 0))

    (entity :id 'blue-picket-0 :archetype 'static-turret :team 'blue
      :position '(-3000 -40000 -2000)
      :rotation '(0 90 0))

    (entity :id 'blue-picket-1 :archetype 'static-turret :team 'blue
      :position '(3000 -40000 2000)
      :rotation '(0 90 0))

    (entity :id 'red-capital-0 :archetype 'capital-ship :team 'red
      :position '(-40000 70000 5000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-1 :archetype 'capital-ship :team 'red
      :position '(40000 70000 -5000)
      :rotation '(0 -90 0))))
