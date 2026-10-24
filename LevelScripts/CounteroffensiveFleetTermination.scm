(load-script "Libraries/objectives.scm")
(load-script "Libraries/formations.scm")

(let ((first-reserve-id 'red-capital-4)
      (first-reserve-time 75)
      (second-reserve-id 'red-capital-5)
      (second-reserve-time 150))
  (level
    :id 'counteroffensive-fleet-termination
    :title "Counteroffensive 04: Fleet Termination"
    :description "Destroy the enemy fleet and both reserve carriers before the operation expires."

    :unlock (list
      (level-completed 'counteroffensive-three-axes))

    :player 'player

    :mission (decisive-mission 'blue
      (all-of
        (entity-kills-reached
          '(player blue-capital-0 blue-capital-1 blue-capital-2 blue-capital-3)
          6)
        (destroy-all
          (append
            '(red-capital-0 red-capital-1 red-capital-2 red-capital-3)
            (list first-reserve-id)
            (list second-reserve-id))))
      (any-of
        (destroy-any '(player))
        (time-elapsed 330)))

    :entities (append
      (list
        (entity :id 'player :archetype 'player-fighter :team 'blue
          :position '(0 -95000 8000)
          :rotation '(0 90 0)))

      (named-capitals 'blue '(0 90 0)
        '((blue-capital-0 (-60000 -55000 -6000))
          (blue-capital-1 (-20000 -55000 2000))
          (blue-capital-2 (20000 -55000 -2000))
          (blue-capital-3 (60000 -55000 6000))))

      (named-capitals 'red '(0 -90 0)
        '((red-capital-0 (-75000 75000 8000))
          (red-capital-1 (-25000 75000 -3000))
          (red-capital-2 (25000 75000 3000))
          (red-capital-3 (75000 75000 -8000))))

      (list
        (entity :id first-reserve-id :archetype 'capital-ship :team 'red
          :position '(-35000 160000 8000)
          :rotation '(0 -90 0)
          :spawn-at first-reserve-time))

      (list
        (entity :id second-reserve-id :archetype 'capital-ship :team 'red
          :position '(35000 180000 -8000)
          :rotation '(0 -90 0)
          :spawn-at second-reserve-time)))))
