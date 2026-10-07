(level
  :id 'border-skirmish
  :title "Border Skirmish"
  :description "A two-team encounter demonstrating scripted level construction."

  :teams '(blue
    red)

  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :heroes '(player blue-capital)
    :must-survive '(blue-capital)
    :required-kills '(red-capital))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -25000 1000)
      :rotation '(0 90 0))

    (entity :id 'blue-capital :archetype 'capital-ship :team 'blue
      :position '(-40000 0 0)
      :rotation '(0 0 0))

    (entity :id 'red-capital :archetype 'capital-ship :team 'red
      :position '(40000 0 0)
      :rotation '(0 180 0))

    (entity :id 'red-turret :archetype 'static-turret :team 'red
      :position '(30000 15000 0)
      :rotation '(0 180 0))))
