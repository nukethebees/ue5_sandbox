(level
  :id 'defence-last-anchorage
  :title "Defence 04: Last Anchorage"
  :description "Hold the fleet anchorage against three carriers and a delayed reserve formation."

  :unlock (list
    (level-completed 'defence-dual-custody))

  :teams '(blue
    red)

  :player 'player

  :mission (mission
    :mode 'survive-time
    :time-limit 150
    :must-survive '(player blue-capital-0 blue-capital-1))

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

    (entity :id 'blue-picket-0 :archetype 'static-turret :team 'blue
      :position '(-45000 -38000 -2500)
      :rotation '(0 90 0))

    (entity :id 'blue-picket-1 :archetype 'static-turret :team 'blue
      :position '(-30000 -36000 2500)
      :rotation '(0 90 0))

    (entity :id 'blue-picket-2 :archetype 'static-turret :team 'blue
      :position '(-15000 -38000 -2500)
      :rotation '(0 90 0))

    (entity :id 'blue-picket-3 :archetype 'static-turret :team 'blue
      :position '(15000 -38000 2500)
      :rotation '(0 90 0))

    (entity :id 'blue-picket-4 :archetype 'static-turret :team 'blue
      :position '(30000 -36000 -2500)
      :rotation '(0 90 0))

    (entity :id 'blue-picket-5 :archetype 'static-turret :team 'blue
      :position '(45000 -38000 2500)
      :rotation '(0 90 0))

    (entity :id 'red-capital-0 :archetype 'capital-ship :team 'red
      :position '(-55000 70000 6000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-1 :archetype 'capital-ship :team 'red
      :position '(0 70000 0)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-2 :archetype 'capital-ship :team 'red
      :position '(55000 70000 -6000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-3 :archetype 'capital-ship :team 'red
      :position '(-25000 150000 8000)
      :rotation '(0 -90 0)
      :spawn-at 75)))
