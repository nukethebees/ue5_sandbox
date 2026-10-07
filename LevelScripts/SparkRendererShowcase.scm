(level
  :id 'spark-renderer-showcase
  :title "Spark Renderer Showcase"
  :description "An autonomous close-range crossfire staged to showcase analytic laser-impact sparks."

  :teams '(blue
    red)

  :camera (camera
    :look-at '(blue-capital-left blue-capital-right
             red-capital-left red-capital-right)
    :distance 150000
    :offset-direction '(-1 -1 0.7))

  :entities (list
    (entity :id 'blue-capital-left :archetype 'capital-ship :team 'blue
      :position '(-35000 -65000 -6000)
      :rotation '(0 90 0))

    (entity :id 'blue-capital-right :archetype 'capital-ship :team 'blue
      :position '(35000 -65000 6000)
      :rotation '(0 90 0))

    (entity :id 'blue-turret-0 :archetype 'static-turret :team 'blue
      :position '(-45000 -25000 -3000)
      :rotation '(0 90 0))

    (entity :id 'blue-turret-1 :archetype 'static-turret :team 'blue
      :position '(-15000 -25000 3000)
      :rotation '(0 90 0))

    (entity :id 'blue-turret-2 :archetype 'static-turret :team 'blue
      :position '(15000 -25000 -3000)
      :rotation '(0 90 0))

    (entity :id 'blue-turret-3 :archetype 'static-turret :team 'blue
      :position '(45000 -25000 3000)
      :rotation '(0 90 0))

    (entity :id 'red-turret-0 :archetype 'static-turret :team 'red
      :position '(-45000 25000 3000)
      :rotation '(0 -90 0))

    (entity :id 'red-turret-1 :archetype 'static-turret :team 'red
      :position '(-15000 25000 -3000)
      :rotation '(0 -90 0))

    (entity :id 'red-turret-2 :archetype 'static-turret :team 'red
      :position '(15000 25000 3000)
      :rotation '(0 -90 0))

    (entity :id 'red-turret-3 :archetype 'static-turret :team 'red
      :position '(45000 25000 -3000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-left :archetype 'capital-ship :team 'red
      :position '(-35000 65000 6000)
      :rotation '(0 -90 0))

    (entity :id 'red-capital-right :archetype 'capital-ship :team 'red
      :position '(35000 65000 -6000)
      :rotation '(0 -90 0))))
