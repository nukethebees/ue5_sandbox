(load-script "Libraries/objectives.scm")
(level
  :id 'defence-screen-duty
  :title "Defence 01: Screen Duty"
  :description "Keep the assigned capital ship operational until the enemy attack window closes."

  :unlock (list
    (level-completed 'strike-reserve-breakthrough))

  :player 'player

  :mission (decisive-mission 'blue
    (time-elapsed 75)
    (destroy-any '(player blue-capital)))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -95000 8000)
      :rotation '(0 90 0))

    (entity :id 'blue-capital :archetype 'capital-ship :team 'blue
      :position '(0 -50000 0)
      :rotation '(0 90 0))

    (entity :id 'red-capital :archetype 'capital-ship :team 'red
      :position '(0 70000 0)
      :rotation '(0 -90 0))))
