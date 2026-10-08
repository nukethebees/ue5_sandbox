(level
  :id 'strike-carrier-intercept
  :title "Strike 03: Carrier Intercept"
  :description "Support a fleet carrier and destroy an enemy capital ship protected by a forward battery."

  :unlock (list
    (level-completed 'strike-layered-battery))


  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :kill-count 1
    :heroes '(player blue-capital)
    :must-survive '(player)
    :required-kills '(red-capital))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -85000 6000)
      :rotation '(0 90 0))

    (entity :id 'blue-capital :archetype 'capital-ship :team 'blue
      :position '(0 -40000 0)
      :rotation '(0 90 0))

    (entity :id 'red-capital :archetype 'capital-ship :team 'red
      :position '(0 60000 0)
      :rotation '(0 -90 0))

    (entity :id 'red-turret-0 :archetype 'static-turret :team 'red
      :position '(-7000 48000 -2500)
      :rotation '(0 -90 0))

    (entity :id 'red-turret-1 :archetype 'static-turret :team 'red
      :position '(7000 48000 2500)
      :rotation '(0 -90 0))))
