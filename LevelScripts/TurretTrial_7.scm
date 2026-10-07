(level
  :id 'turret-trial-7
  :title "Turret Trial 7"
  :description "Break an eight-turret ring before the 120-second limit expires."

  :unlock (list
    (level-completed 'turret-trial-6))

  :teams '(blue
    red)

  :player 'player

  :mission (mission
    :mode 'kill-enemies-within-time
    :time-limit 120
    :heroes '(player)
    :must-survive '(player))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -74000 1000)
      :rotation '(0 90 0))

    (entity :id 'turret-0 :archetype 'static-turret :team 'red
      :position '(0 4000 1000)
      :rotation '(0 -90 0))

    (entity :id 'turret-1 :archetype 'static-turret :team 'red
      :position '(4243 5757 -1000)
      :rotation '(0 -90 0))

    (entity :id 'turret-2 :archetype 'static-turret :team 'red
      :position '(6000 10000 1000)
      :rotation '(0 -90 0))

    (entity :id 'turret-3 :archetype 'static-turret :team 'red
      :position '(4243 14243 -1000)
      :rotation '(0 -90 0))

    (entity :id 'turret-4 :archetype 'static-turret :team 'red
      :position '(0 16000 1000)
      :rotation '(0 -90 0))

    (entity :id 'turret-5 :archetype 'static-turret :team 'red
      :position '(-4243 14243 -1000)
      :rotation '(0 -90 0))

    (entity :id 'turret-6 :archetype 'static-turret :team 'red
      :position '(-6000 10000 1000)
      :rotation '(0 -90 0))

    (entity :id 'turret-7 :archetype 'static-turret :team 'red
      :position '(-4243 5757 -1000)
      :rotation '(0 -90 0))))
