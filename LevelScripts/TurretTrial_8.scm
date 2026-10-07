(level
  :id 'turret-trial-8
  :title "Turret Trial 8"
  :description "Clear two interlocking turret layers within 105 seconds."

  :unlock (list
    (level-completed 'turret-trial-7))

  :teams '(blue
    red)

  :player 'player

  :mission (mission
    :mode 'kill-enemies-within-time
    :time-limit 105
    :heroes '(player)
    :must-survive '(player))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -75200 1000)
      :rotation '(0 90 0))

    (entity :id 'turret-0 :archetype 'static-turret :team 'red
      :position '(-4000 7000 -2500)
      :rotation '(0 -90 0))

    (entity :id 'turret-1 :archetype 'static-turret :team 'red
      :position '(4000 7000 -2500)
      :rotation '(0 -90 0))

    (entity :id 'turret-2 :archetype 'static-turret :team 'red
      :position '(0 10000 -2500)
      :rotation '(0 -90 0))

    (entity :id 'turret-3 :archetype 'static-turret :team 'red
      :position '(-4000 13000 -2500)
      :rotation '(0 -90 0))

    (entity :id 'turret-4 :archetype 'static-turret :team 'red
      :position '(4000 13000 -2500)
      :rotation '(0 -90 0))

    (entity :id 'turret-5 :archetype 'static-turret :team 'red
      :position '(0 7000 2500)
      :rotation '(0 -90 0))

    (entity :id 'turret-6 :archetype 'static-turret :team 'red
      :position '(-4000 10000 2500)
      :rotation '(0 -90 0))

    (entity :id 'turret-7 :archetype 'static-turret :team 'red
      :position '(4000 10000 2500)
      :rotation '(0 -90 0))

    (entity :id 'turret-8 :archetype 'static-turret :team 'red
      :position '(0 13000 2500)
      :rotation '(0 -90 0))

    (entity :id 'turret-9 :archetype 'static-turret :team 'red
      :position '(0 16000 2500)
      :rotation '(0 -90 0))))
