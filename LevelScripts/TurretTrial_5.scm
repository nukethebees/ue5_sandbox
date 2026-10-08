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

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -74000 1000)
      :rotation '(0 90 0))

    (entity :id 'turret-0 :archetype 'static-turret :team 'red
      :position '(-3000 4000 0)
      :rotation '(0 -90 0))

    (entity :id 'turret-1 :archetype 'static-turret :team 'red
      :position '(3000 7000 1500)
      :rotation '(0 -90 0))

    (entity :id 'turret-2 :archetype 'static-turret :team 'red
      :position '(-3000 10000 -1500)
      :rotation '(0 -90 0))

    (entity :id 'turret-3 :archetype 'static-turret :team 'red
      :position '(3000 13000 1500)
      :rotation '(0 -90 0))

    (entity :id 'turret-4 :archetype 'static-turret :team 'red
      :position '(0 16000 -1500)
      :rotation '(0 -90 0))))
