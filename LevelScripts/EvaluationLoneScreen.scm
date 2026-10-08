(level
  :id 'evaluation-lone-screen
  :title "Evaluation 03: Lone Screen"
  :description "Survive alone through repeated enemy fighter launches for two minutes."

  :unlock (list
    (level-completed 'evaluation-distributed-defence))

  :player 'player

  :mission (mission
    :mode 'survive-time
    :time-limit 120
    :heroes '(player)
    :must-survive '(player))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -120000 0)
      :rotation '(0 90 0))

    (entity :archetype 'capital-ship :team 'red
      :position '(0 130000 0)
      :rotation '(0 -90 0))

    (entity :archetype 'capital-ship :team 'red
      :position '(-85000 175000 6000)
      :rotation '(0 -90 0)
      :spawn-at 35)

    (entity :archetype 'capital-ship :team 'red
      :position '(85000 175000 -6000)
      :rotation '(0 -90 0)
      :spawn-at 75)))
