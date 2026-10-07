(level
  :id 'space-dust-flight-lab
  :title "Space Dust Flight Lab"
  :description "A quiet player-flight lab for tuning velocity-driven space dust. A friendly capital ship provides an optional depth-occlusion reference."

  :teams '(blue)

  :player 'player

  :mission (mission
    :mode 'survive-time
    :time-limit 3600
    :heroes '(player)
    :must-survive '(player))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 0 0)
      :rotation '(0 0 0))

    (entity :id 'occlusion-reference :archetype 'capital-ship :team 'blue
      :position '(50000 0 0)
      :rotation '(0 180 0))))
