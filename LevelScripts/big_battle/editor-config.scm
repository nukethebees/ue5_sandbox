(export scene)
(define scene (level
  :id 'big-battle
  :title "big_battle"
  :description ""
  :player 'bp-testspaceship
  :static-collision (list)
  :entities (list
    (entity
      :id 'bp-testspaceship
      :archetype 'player-fighter
      :team 'white
      :position '(-196590.0 48450.0 0.0))
    (entity
      :id 'capitalship-blue-0
      :archetype 'capital-ship
      :team 'blue
      :position '(731170.0 622950.0 155150.0)
      :rotation '(0.0 50.00000000000006 -0.0))
    (entity
      :id 'capitalship-blue-1
      :archetype 'capital-ship
      :team 'blue
      :position '(702380.0 554930.0 155150.0)
      :rotation '(0.0 50.00000000000006 -0.0))
    (entity
      :id 'capitalship-green-0
      :archetype 'capital-ship
      :team 'green
      :position '(-8390.0 217420.0 0.0)
      :rotation '(2.286061858078901e-14 49.999999999998984 1.0075417770579342e-14))
    (entity
      :id 'capitalship-green-1
      :archetype 'capital-ship
      :team 'green
      :position '(5890.0 243240.0 0.0)
      :rotation '(0.0 50.00000000000006 -0.0))
    (entity
      :id 'capitalship-orange-0
      :archetype 'capital-ship
      :team 'orange
      :position '(765580.0 -641670.0 155150.0)
      :rotation '(0.0 50.00000000000006 -0.0))
    (entity
      :id 'capitalship-orange-1
      :archetype 'capital-ship
      :team 'orange
      :position '(731060.0 -726460.0 155150.0)
      :rotation '(0.0 50.00000000000006 -0.0))
    (entity
      :id 'capitalship-red-0
      :archetype 'capital-ship
      :team 'red
      :position '(176790.0 -264260.0 0.0)
      :rotation '(0.0 50.00000000000006 -0.0))
    (entity
      :id 'capitalship-red-1
      :archetype 'capital-ship
      :team 'red
      :position '(211310.0 -226160.0 0.0)
      :rotation '(0.0 50.00000000000006 -0.0))
    (entity
      :id 'capitalship-red-2
      :archetype 'capital-ship
      :team 'red
      :position '(221050.0 -307550.0 0.0)
      :rotation '(0.0 50.00000000000006 -0.0))
    (entity
      :id 'capitalship-red-3
      :archetype 'capital-ship
      :team 'red
      :position '(255570.0 -269450.0 0.0)
      :rotation '(0.0 50.00000000000006 -0.0))
    (entity
      :id 'capitalship-red-4
      :archetype 'capital-ship
      :team 'red
      :position '(221050.0 -312970.0 43970.0)
      :rotation '(0.0 50.00000000000006 -0.0))
    (entity
      :id 'capitalship-red-5
      :archetype 'capital-ship
      :team 'red
      :position '(255570.0 -274870.0 43970.0)
      :rotation '(0.0 50.00000000000006 -0.0))
    (entity
      :id 'capitalship-red-6
      :archetype 'capital-ship
      :team 'red
      :position '(1427050.0 105610.0 43970.0)
      :rotation '(0.0 50.00000000000006 -0.0))
    (entity
      :id 'fighter-blue-0
      :archetype 'fighter
      :team 'blue
      :position '(580410.0 599600.0 159290.0))
    (entity
      :id 'fighter-blue-1
      :archetype 'fighter
      :team 'blue
      :position '(581080.0 588860.0 159290.0))
    (entity
      :id 'fighter-blue-2
      :archetype 'fighter
      :team 'blue
      :position '(582130.0 577180.0 159290.0))
    (entity
      :id 'fighter-blue-3
      :archetype 'fighter
      :team 'blue
      :position '(570160.0 583790.0 159290.0))
    (entity
      :id 'fighter-blue-4
      :archetype 'fighter
      :team 'blue
      :position '(570650.0 595730.0 159290.0))
    (entity
      :id 'fighter-blue-5
      :archetype 'fighter
      :team 'blue
      :position '(560560.0 590490.0 159290.0))
    (entity
      :id 'fighter-blue-6
      :archetype 'fighter
      :team 'blue
      :position '(561290.0 603240.0 159290.0))
    (entity
      :id 'fighter-blue-7
      :archetype 'fighter
      :team 'blue
      :position '(582130.0 577180.0 177180.0))
    (entity
      :id 'fighter-green-0
      :archetype 'fighter
      :team 'green
      :position '(-40370.0 142770.0 5810.0))
    (entity
      :id 'fighter-green-1
      :archetype 'fighter
      :team 'green
      :position '(-40370.0 142770.0 23700.0))
    (entity
      :id 'fighter-green-10
      :archetype 'fighter
      :team 'green
      :position '(-50130.0 138900.0 41310.0))
    (entity
      :id 'fighter-green-11
      :archetype 'fighter
      :team 'green
      :position '(-39700.0 132030.0 5810.000000000004))
    (entity
      :id 'fighter-green-12
      :archetype 'fighter
      :team 'green
      :position '(-60220.0 133660.0 41310.0))
    (entity
      :id 'fighter-green-13
      :archetype 'fighter
      :team 'green
      :position '(-59490.0 146410.0 41310.0))
    (entity
      :id 'fighter-green-14
      :archetype 'fighter
      :team 'green
      :position '(-38650.0 120350.0 5810.0))
    (entity
      :id 'fighter-green-15
      :archetype 'fighter
      :team 'green
      :position '(-50620.0 126960.0 5810.0))
    (entity
      :id 'fighter-green-16
      :archetype 'fighter
      :team 'green
      :position '(-50130.0 138900.0 5810.0))
    (entity
      :id 'fighter-green-17
      :archetype 'fighter
      :team 'green
      :position '(-60220.0 133660.0 5810.0))
    (entity
      :id 'fighter-green-18
      :archetype 'fighter
      :team 'green
      :position '(-59490.0 146410.0 5810.0))
    (entity
      :id 'fighter-green-19
      :archetype 'fighter
      :team 'green
      :position '(-38650.0 120350.0 23700.0))
    (entity
      :id 'fighter-green-2
      :archetype 'fighter
      :team 'green
      :position '(-50620.0 126960.0 23700.0))
    (entity
      :id 'fighter-green-20
      :archetype 'fighter
      :team 'green
      :position '(-39700.0 132030.0 23700.0))
    (entity
      :id 'fighter-green-3
      :archetype 'fighter
      :team 'green
      :position '(-50130.0 138900.0 23700.0))
    (entity
      :id 'fighter-green-4
      :archetype 'fighter
      :team 'green
      :position '(-60220.0 133660.0 23700.0))
    (entity
      :id 'fighter-green-5
      :archetype 'fighter
      :team 'green
      :position '(-59490.0 146410.0 23700.0))
    (entity
      :id 'fighter-green-6
      :archetype 'fighter
      :team 'green
      :position '(-38650.0 120350.0 41310.0))
    (entity
      :id 'fighter-green-7
      :archetype 'fighter
      :team 'green
      :position '(-39700.0 132030.0 41310.0))
    (entity
      :id 'fighter-green-8
      :archetype 'fighter
      :team 'green
      :position '(-40370.0 142770.0 41310.0))
    (entity
      :id 'fighter-green-9
      :archetype 'fighter
      :team 'green
      :position '(-50620.0 126960.0 41310.0)))
  :settings (settings)))
