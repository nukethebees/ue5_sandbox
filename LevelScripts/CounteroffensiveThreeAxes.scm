(load-script "Libraries/objectives.scm")
(load-script "Libraries/formations.scm")

(let ((reserve-id 'red-capital-4)
      (reserve-time 75))
  (level
    :id 'counteroffensive-three-axes
    :title "Counteroffensive 03: Three Axes"
    :description "Destroy a dispersed capital force approaching across three axes and its delayed reserve."

    :unlock (list
      (level-completed 'counteroffensive-second-echelon))

    :player 'player

    :mission (decisive-mission 'blue
    (all-of (entity-kills-reached '(player blue-capital-0 blue-capital-1 blue-capital-2) 5) (destroy-all (append '(red-capital-0 red-capital-1 red-capital-2 red-capital-3) (list reserve-id))))
    (any-of (destroy-any '(player)) (time-elapsed 270)))



    :entities (append
      (list
        (entity :id 'player :archetype 'player-fighter :team 'blue
          :position '(0 -95000 8000)
          :rotation '(0 90 0)))

      (named-capitals 'blue '(0 90 0)
        '((blue-capital-0 (-40000 -55000 -5000))
          (blue-capital-1 (0 -55000 0))
          (blue-capital-2 (40000 -55000 5000))))

      (list
        (entity :id 'red-capital-0 :archetype 'capital-ship :team 'red
          :position '(-90000 50000 8000)
          :rotation '(0 -70 0)))

      (list
        (entity :id 'red-capital-1 :archetype 'capital-ship :team 'red
          :position '(90000 50000 -8000)
          :rotation '(0 -110 0)))

      (named-capitals 'red '(0 -90 0)
        '((red-capital-2 (-30000 100000 -4000))
          (red-capital-3 (30000 100000 4000))))

      (list
        (entity :id reserve-id :archetype 'capital-ship :team 'red
          :position '(0 170000 0)
          :rotation '(0 -90 0)
          :spawn-at reserve-time)))))
