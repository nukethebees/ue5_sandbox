(level
  :id 'strike-range-clearance
  :title "Strike 01: Range Clearance"
  :description "Eliminate an isolated three-turret battery and return to operational control."


  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :heroes '(player)
    :must-survive '(player))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -75000 1000)
      :rotation '(0 90 0))

    (entity :id 'turret-0 :archetype 'static-turret :team 'red
      :position '(-6000 15000 -1500)
      :rotation '(0 -90 0))

    (entity :id 'turret-1 :archetype 'static-turret :team 'red
      :position '(0 15000 1500)
      :rotation '(0 -90 0))

    (entity :id 'turret-2 :archetype 'static-turret :team 'red
      :position '(6000 15000 -1500)
      :rotation '(0 -90 0))))
