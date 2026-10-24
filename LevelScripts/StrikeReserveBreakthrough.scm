(load-script "Libraries/objectives.scm")
(load-script "Libraries/formations.scm")

(let ((reserve-id 'red-capital-2)
      (reserve-time 60))
  (level
    :id 'strike-reserve-breakthrough
    :title "Strike 04: Reserve Breakthrough"
    :description "Destroy two enemy capital ships, then neutralise the reserve carrier entering the battlespace."

    :unlock (list
      (level-completed 'strike-carrier-intercept))

    :player 'player

    :mission (decisive-mission 'blue
    (all-of (entity-kills-reached '(player blue-capital-0 blue-capital-1) 3) (destroy-all (append '(red-capital-0 red-capital-1) (list reserve-id))))
    (destroy-any '(player)))



    :entities (append
      (list
        (entity :id 'player :archetype 'player-fighter :team 'blue
          :position '(0 -95000 8000)
          :rotation '(0 90 0)))

      (named-capitals 'blue '(0 90 0)
        '((blue-capital-0 (-30000 -50000 -4000))
          (blue-capital-1 (30000 -50000 4000))))

      (named-capitals 'red '(0 -90 0)
        '((red-capital-0 (-30000 70000 4000))
          (red-capital-1 (30000 70000 -4000))))

      (list
        (entity :id reserve-id :archetype 'capital-ship :team 'red
          :position '(0 150000 8000)
          :rotation '(0 -90 0)
          :spawn-at reserve-time)))))
