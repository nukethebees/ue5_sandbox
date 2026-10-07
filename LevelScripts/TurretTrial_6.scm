(level
  :id 'turret-trial-6
  :title "Turret Trial 6"
  :description "Choose which of two mutually supporting turret clusters to attack first."

  :unlock (list
    (level-completed 'turret-trial-5))

  :teams '(blue
    red)

  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :heroes '(player)
    :must-survive '(player))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -73000 1000)
      :rotation '(0 90 0))

    (entity :id 'turret-0 :archetype 'static-turret :team 'red
      :position '(-7000 7500 0)
      :rotation '(0 -90 0))

    (entity :id 'turret-1 :archetype 'static-turret :team 'red
      :position '(-4000 9500 2000)
      :rotation '(0 -90 0))

    (entity :id 'turret-2 :archetype 'static-turret :team 'red
      :position '(-7000 11500 -2000)
      :rotation '(0 -90 0))

    (entity :id 'turret-3 :archetype 'static-turret :team 'red
      :position '(7000 7500 0)
      :rotation '(0 -90 0))

    (entity :id 'turret-4 :archetype 'static-turret :team 'red
      :position '(4000 9500 -2000)
      :rotation '(0 -90 0))

    (entity :id 'turret-5 :archetype 'static-turret :team 'red
      :position '(7000 11500 2000)
      :rotation '(0 -90 0))))
