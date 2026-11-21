(export scene)
(define scene (level
  :id 'big-battle
  :title "big_battle"
  :description ""
  :player 'bp-testspaceship
  :entities (list
    (entity
      :id 'bp-testspaceship
      :archetype 'player-fighter
      :team 'white
      :position '(-196590.0 48450.0 0.0))
    (entity
      :id 'capitalship-blue
      :archetype 'capital-ship
      :team 'blue
      :position '(736900.0 639720.0 155150.0)
      :rotation '(0.0 50.00000000000005 -0.0))
    (entity
      :id 'capitalship-blue2
      :archetype 'capital-ship
      :team 'blue
      :position '(702380.0 554930.0 155150.0)
      :rotation '(0.0 50.00000000000005 -0.0))
    (entity
      :id 'capitalship-blue3
      :archetype 'capital-ship
      :team 'orange
      :position '(765580.0 -641670.0 155150.0)
      :rotation '(0.0 50.00000000000005 -0.0))
    (entity
      :id 'capitalship-blue4
      :archetype 'capital-ship
      :team 'orange
      :position '(731060.0 -726460.0 155150.0)
      :rotation '(0.0 50.00000000000005 -0.0))
    (entity
      :id 'capitalship-green
      :archetype 'capital-ship
      :team 'green
      :position '(-8390.0 217420.0 0.0)
      :rotation '(2.286061858078909e-14 49.999999999998984 1.0075417770579352e-14))
    (entity
      :id 'capitalship-green2
      :archetype 'capital-ship
      :team 'green
      :position '(5890.0 243240.0 0.0)
      :rotation '(0.0 50.00000000000005 -0.0))
    (entity
      :id 'capitalship-red
      :archetype 'capital-ship
      :team 'red
      :position '(176790.0 -264260.0 0.0)
      :rotation '(0.0 50.00000000000005 -0.0))
    (entity
      :id 'capitalship-red2
      :archetype 'capital-ship
      :team 'red
      :position '(211310.0 -226160.0 0.0)
      :rotation '(0.0 50.00000000000005 -0.0))
    (entity
      :id 'fighter
      :archetype 'fighter
      :team 'green
      :position '(-40370.0 142770.0 5810.0))
    (entity
      :id 'fighter2
      :archetype 'fighter
      :team 'green
      :position '(-39700.0 132030.0 5810.000000000004))
    (entity
      :id 'fighter3
      :archetype 'fighter
      :team 'green
      :position '(-38650.0 120350.0 5810.0))
    (entity
      :id 'fighter4
      :archetype 'fighter
      :team 'green
      :position '(-50620.0 126960.0 5810.0))
    (entity
      :id 'fighter5
      :archetype 'fighter
      :team 'green
      :position '(-50130.0 138900.0 5810.0))
    (entity
      :id 'fighter6
      :archetype 'fighter
      :team 'green
      :position '(-60220.0 133660.0 5810.0))
    (entity
      :id 'fighter7
      :archetype 'fighter
      :team 'green
      :position '(-59490.0 146410.0 5810.0)))
  :settings (settings)))
