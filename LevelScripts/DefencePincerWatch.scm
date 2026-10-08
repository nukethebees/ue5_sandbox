(level
  :id 'defence-pincer-watch
  :title "Defence 02: Pincer Watch"
  :description "Protect a capital ship and its forward pickets from a two-axis carrier attack."

  :unlock (list
    (level-completed 'defence-screen-duty))

  :player 'player

  :mission (mission
    :mode 'survive-time
    :time-limit 90
    :must-survive '(player blue-capital))

  :entities (append
    (list
      (entity :id 'player :archetype 'player-fighter :team 'blue
        :position '(0 -95000 8000)
        :rotation '(0 90 0)))

    (list
      (entity :id 'blue-capital :archetype 'capital-ship :team 'blue
        :position '(0 -50000 0)
        :rotation '(0 90 0)))

    (map
      (lambda (position)
        (entity :archetype 'static-turret :team 'blue
          :position position
          :rotation '(0 90 0)))
      '((-3000 -40000 -2000)
        (3000 -40000 2000)))

    (map
      (lambda (position)
        (entity :archetype 'capital-ship :team 'red
          :position position
          :rotation '(0 -90 0)))
      '((-40000 70000 5000)
        (40000 70000 -5000)))))
