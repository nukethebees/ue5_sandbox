(load-script "Libraries/objectives.scm")
(level
  :id 'border-skirmish
  :title "Border Skirmish"
  :description "A two-team encounter demonstrating scripted level construction."

  :player 'player

  :mission (decisive-mission 'blue
    (all-of
      (entity-kills-reached
        '(player blue-capital)
        2)
      (destroy-all '(red-capital)))
    (destroy-any '(blue-capital)))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -25000 1000)
      :rotation '(0 90 0))

    (entity :id 'blue-capital :archetype 'capital-ship :team 'blue
      :position '(-40000 0 0))

    (entity :id 'red-capital :archetype 'capital-ship :team 'red
      :position '(40000 0 0)
      :rotation '(0 180 0))

    (entity :id 'red-turret :archetype 'static-turret :team 'red
      :position '(30000 15000 0)
      :rotation '(0 180 0))))
