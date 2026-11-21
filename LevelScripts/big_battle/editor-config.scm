(export scene)
(define scene (level
  :id 'big-battle
  :title "big_battle"
  :description ""
  :par-time :none
  :unlock :none
  :player 'bp-testspaceship
  :camera :none
  :entities (list
    (entity
      :id 'bp-testspaceship
      :archetype 'player-fighter
      :team 'white
      :position '(0.0 0.0 0.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'capitalship-blue
      :archetype 'capital-ship
      :team 'blue
      :position '(736900.0 639720.0 155150.0)
      :rotation '(0.0 50.0 -0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'capitalship-blue2
      :archetype 'capital-ship
      :team 'blue
      :position '(702380.0 554930.0 155150.0)
      :rotation '(0.0 50.0 -0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'capitalship-blue3
      :archetype 'capital-ship
      :team 'orange
      :position '(765580.0 -641670.0 155150.0)
      :rotation '(0.0 50.0 -0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'capitalship-blue4
      :archetype 'capital-ship
      :team 'orange
      :position '(731060.0 -726460.0 155150.0)
      :rotation '(0.0 50.0 -0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'capitalship-green
      :archetype 'capital-ship
      :team 'green
      :position '(54540.0 -62440.0 0.0)
      :rotation '(2.286061858078914e-14 49.999999999999 1.0075417770579363e-14)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'capitalship-green2
      :archetype 'capital-ship
      :team 'green
      :position '(68820.0 -36620.0 0.0)
      :rotation '(0.0 50.0 -0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'capitalship-red
      :archetype 'capital-ship
      :team 'red
      :position '(176790.0 -264260.0 0.0)
      :rotation '(0.0 50.0 -0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'capitalship-red2
      :archetype 'capital-ship
      :team 'red
      :position '(211310.0 -226160.0 0.0)
      :rotation '(0.0 50.0 -0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none))
  :settings (settings)))
