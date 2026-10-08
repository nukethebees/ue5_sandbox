(level
  :id 'defence-dual-custody
  :title "Defence 03: Dual Custody"
  :description "Maintain two capital ships through the initial assault and a reserve carrier arrival."

  :unlock (list
    (level-completed 'defence-pincer-watch))


  :player 'player

  :mission (mission
    :mode 'survive-time
    :time-limit 120
    :must-survive '(player blue-capital-0 blue-capital-1))

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

    (entity :id 'blue-picket-0 :archetype 'static-turret :team 'blue
      :position '(-32000 -38000 -2000)
      :rotation '(0 90 0))

    (entity :id 'blue-picket-1 :archetype 'static-turret :team 'blue
      :position '(-18000 -38000 2000)
      :rotation '(0 90 0))

    (entity :id 'blue-picket-2 :archetype 'static-turret :team 'blue
      :position '(18000 -38000 -2000)
      :rotation '(0 90 0))

    (entity :id 'blue-picket-3 :archetype 'static-turret :team 'blue
      :position '(32000 -38000 2000)
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
