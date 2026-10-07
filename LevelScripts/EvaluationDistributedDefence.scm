(level
  :id 'evaluation-distributed-defence
  :title "Evaluation 02: Distributed Defence"
  :description "Hold three separated defence installations against a converging capital-ship attack."

  :unlock (list
    (level-completed 'evaluation-break-the-spear))

  :teams '(blue
    red)

  :player 'player

  :mission (mission
    :mode 'survive-time
    :time-limit 180
    :heroes '(player blue-installation-west blue-installation-centre blue-installation-east)
    :must-survive '(player blue-installation-west blue-installation-centre blue-installation-east))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -105000 7000)
      :rotation '(0 90 0))

    (entity :id 'blue-installation-west :archetype 'static-turret :team 'blue
      :position '(-110000 -20000 0)
      :rotation '(0 90 0))

    (entity :id 'blue-installation-centre :archetype 'static-turret :team 'blue
      :position '(0 0 0)
      :rotation '(0 90 0))

    (entity :id 'blue-installation-east :archetype 'static-turret :team 'blue
      :position '(110000 -20000 0)
      :rotation '(0 90 0))

    (entity :id 'blue-capital-0 :archetype 'capital-ship :team 'blue
      :position '(0 -55000 -6000)
      :rotation '(0 90 0))

    (entity :id 'red-capital-west :archetype 'capital-ship :team 'red
      :position '(-150000 85000 6000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-centre :archetype 'capital-ship :team 'red
      :position '(0 115000 -6000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-east :archetype 'capital-ship :team 'red
      :position '(150000 85000 6000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-reserve-west :archetype 'capital-ship :team 'red
      :position '(-90000 165000 -6000)
      :rotation '(0 -90 0)
      :spawn-at 70)

    (entity :id 'red-capital-reserve-east :archetype 'capital-ship :team 'red
      :position '(90000 165000 -6000)
      :rotation '(0 -90 0)
      :spawn-at 120)))
