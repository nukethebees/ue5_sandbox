(level
  :id 'fleet-overview
  :title "Fleet Overview"
  :description "A playerless battle viewed from an authored camera between two flagships."


  :camera (camera
    :look-at '(blue-capital red-capital)
    :distance 180000
    :offset-direction '(-1 -1 0.6))

  :entities (list
    (entity :id 'blue-capital :archetype 'capital-ship :team 'blue
      :position '(-70000 0 0)
      :rotation '(0 0 0))

    (entity :id 'blue-turret :archetype 'static-turret :team 'blue
      :position '(-55000 -25000 0)
      :rotation '(0 0 0))

    (entity :id 'red-capital :archetype 'capital-ship :team 'red
      :position '(70000 0 0)
      :rotation '(0 180 0))

    (entity :id 'red-turret :archetype 'static-turret :team 'red
      :position '(55000 25000 0)
      :rotation '(0 180 0))))
