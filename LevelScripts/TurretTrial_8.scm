(level
  :id 'turret-trial-8
  :title "Turret Trial 8"
  :description "Clear two interlocking turret layers within 105 seconds."

  :unlock (list
    (level-completed 'turret-trial-7))

  :player 'player

  :mission (mission
    :mode 'kill-enemies-within-time
    :time-limit 105
    :heroes '(player)
    :must-survive '(player))

  :entities (cons
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -75200 1000)
      :rotation '(0 90 0))

    (map
      (lambda (position)
        (entity :archetype 'static-turret :team 'red
          :position position
          :rotation '(0 -90 0)))
      '((-4000 7000 -2500)
        (4000 7000 -2500)
        (0 10000 -2500)
        (-4000 13000 -2500)
        (4000 13000 -2500)
        (0 7000 2500)
        (-4000 10000 2500)
        (4000 10000 2500)
        (0 13000 2500)
        (0 16000 2500)))))
