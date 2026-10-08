(load-script "Libraries/formations.scm")

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

  :entities (append
    (list
      (entity :id 'player :archetype 'player-fighter :team 'blue
        :position '(0 -95000 8000)
        :rotation '(0 90 0)))

    (named-capitals 'blue '(0 90 0)
      '((blue-capital-0 (-25000 -50000 -4000))
        (blue-capital-1 (25000 -50000 4000))))

    (entities-at 'static-turret 'blue '(0 90 0)
      '((-32000 -38000 -2000)
        (-18000 -38000 2000)
        (18000 -38000 -2000)
        (32000 -38000 2000)))

    (entities-at 'capital-ship 'red '(0 -90 0)
      '((-30000 70000 4000)
        (30000 70000 -4000)))

    (list
      (entity :archetype 'capital-ship :team 'red
        :position '(0 150000 8000)
        :rotation '(0 -90 0)
        :spawn-at 60))))
