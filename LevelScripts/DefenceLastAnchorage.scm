(load-script "Libraries/formations.scm")

(level
  :id 'defence-last-anchorage
  :title "Defence 04: Last Anchorage"
  :description "Hold the fleet anchorage against three carriers and a delayed reserve formation."

  :unlock (list
    (level-completed 'defence-dual-custody))

  :player 'player

  :mission (mission
    :mode 'survive-time
    :time-limit 150
    :must-survive '(player blue-capital-0 blue-capital-1))

  :entities (append
    (list
      (entity :id 'player :archetype 'player-fighter :team 'blue
        :position '(0 -95000 8000)
        :rotation '(0 90 0)))

    (named-capitals 'blue '(0 90 0)
      '((blue-capital-0 (-30000 -50000 -4000))
        (blue-capital-1 (30000 -50000 4000))))

    (entities-at 'static-turret 'blue '(0 90 0)
      '((-45000 -38000 -2500)
        (-30000 -36000 2500)
        (-15000 -38000 -2500)
        (15000 -38000 2500)
        (30000 -36000 -2500)
        (45000 -38000 2500)))

    (entities-at 'capital-ship 'red '(0 -90 0)
      '((-55000 70000 6000)
        (0 70000 0)
        (55000 70000 -6000)))

    (list
      (entity :archetype 'capital-ship :team 'red
        :position '(-25000 150000 8000)
        :rotation '(0 -90 0)
        :spawn-at 75))))
