(level
  :id 'spark-renderer-showcase
  :title "Spark Renderer Showcase"
  :description "An autonomous close-range crossfire staged to showcase analytic laser-impact sparks."

  :camera (camera
    :look-at '(blue-capital-left blue-capital-right
             red-capital-left red-capital-right)
    :distance 150000
    :offset-direction '(-1 -1 0.7))

  :entities (append
    (list
      (entity :id 'blue-capital-left :archetype 'capital-ship :team 'blue
        :position '(-35000 -65000 -6000)
        :rotation '(0 90 0)))

    (list
      (entity :id 'blue-capital-right :archetype 'capital-ship :team 'blue
        :position '(35000 -65000 6000)
        :rotation '(0 90 0)))

    (map
      (lambda (position)
        (entity :archetype 'static-turret :team 'blue
          :position position
          :rotation '(0 90 0)))
      '((-45000 -25000 -3000)
        (-15000 -25000 3000)
        (15000 -25000 -3000)
        (45000 -25000 3000)))

    (map
      (lambda (position)
        (entity :archetype 'static-turret :team 'red
          :position position
          :rotation '(0 -90 0)))
      '((-45000 25000 3000)
        (-15000 25000 -3000)
        (15000 25000 3000)
        (45000 25000 -3000)))

    (list
      (entity :id 'red-capital-left :archetype 'capital-ship :team 'red
        :position '(-35000 65000 6000)
        :rotation '(0 -90 0)))

    (list
      (entity :id 'red-capital-right :archetype 'capital-ship :team 'red
        :position '(35000 65000 -6000)
        :rotation '(0 -90 0)))))
