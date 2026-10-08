(level
  :id 'turret-trial-4
  :title "Turret Trial 4"
  :description "Dismantle a diamond formation with overlapping coverage."

  :unlock (list
    (level-completed 'turret-trial-3))


  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :heroes '(player)
    :must-survive '(player))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -74000 1000)
      :rotation '(0 90 0))

    (entity :id 'turret-0 :archetype 'static-turret :team 'red
      :position '(-4000 10000 0)
      :rotation '(0 -90 0))

    (entity :id 'turret-1 :archetype 'static-turret :team 'red
      :position '(4000 10000 0)
      :rotation '(0 -90 0))

    (entity :id 'turret-2 :archetype 'static-turret :team 'red
      :position '(0 6000 2000)
      :rotation '(0 -90 0))

    (entity :id 'turret-3 :archetype 'static-turret :team 'red
      :position '(0 14000 -2000)
      :rotation '(0 -90 0))))
