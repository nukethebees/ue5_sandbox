(level
  :id 'turret-trial-5
  :title "Turret Trial 5"
  :description "Push through a staggered turret corridor without becoming boxed in."

  :unlock (list
    (level-completed 'turret-trial-4))

  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :heroes '(player)
    :must-survive '(player))

  :entities (cons
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -74000 1000)
      :rotation '(0 90 0))

    (map
      (lambda (position)
        (entity :archetype 'static-turret :team 'red
          :position position
          :rotation '(0 -90 0)))
      '((-3000 4000 0)
        (3000 7000 1500)
        (-3000 10000 -1500)
        (3000 13000 1500)
        (0 16000 -1500)))))
