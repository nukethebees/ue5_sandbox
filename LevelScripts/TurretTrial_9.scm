(level
  :id 'turret-trial-9
  :title "Turret Trial 9"
  :description "Destroy a dense double ring of twelve turrets within 90 seconds."

  :unlock (list
    (level-completed 'turret-trial-8))


  :player 'player

  :mission (mission
    :mode 'kill-enemies-within-time
    :time-limit 90
    :heroes '(player)
    :must-survive '(player))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -74000 1000)
      :rotation '(0 90 0))

    (entity :id 'turret-0 :archetype 'static-turret :team 'red
      :position '(0 5500 -2200)
      :rotation '(0 -90 0))

    (entity :id 'turret-1 :archetype 'static-turret :team 'red
      :position '(3897 7750 -2200)
      :rotation '(0 -90 0))

    (entity :id 'turret-2 :archetype 'static-turret :team 'red
      :position '(3897 12250 -2200)
      :rotation '(0 -90 0))

    (entity :id 'turret-3 :archetype 'static-turret :team 'red
      :position '(0 14500 -2200)
      :rotation '(0 -90 0))

    (entity :id 'turret-4 :archetype 'static-turret :team 'red
      :position '(-3897 12250 -2200)
      :rotation '(0 -90 0))

    (entity :id 'turret-5 :archetype 'static-turret :team 'red
      :position '(-3897 7750 -2200)
      :rotation '(0 -90 0))

    (entity :id 'turret-6 :archetype 'static-turret :team 'red
      :position '(2250 6103 2200)
      :rotation '(0 -90 0))

    (entity :id 'turret-7 :archetype 'static-turret :team 'red
      :position '(4500 10000 2200)
      :rotation '(0 -90 0))

    (entity :id 'turret-8 :archetype 'static-turret :team 'red
      :position '(2250 13897 2200)
      :rotation '(0 -90 0))

    (entity :id 'turret-9 :archetype 'static-turret :team 'red
      :position '(-2250 13897 2200)
      :rotation '(0 -90 0))

    (entity :id 'turret-10 :archetype 'static-turret :team 'red
      :position '(-4500 10000 2200)
      :rotation '(0 -90 0))

    (entity :id 'turret-11 :archetype 'static-turret :team 'red
      :position '(-2250 6103 2200)
      :rotation '(0 -90 0))))
