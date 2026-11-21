(export scene)

(define scene (level
  :id 'defend-capital
  :title "defend_capital"
  :description ""
  :player 'playership
  :settings (settings)
  :entities (list
    (entity
      :id 'capitalship-blue
      :archetype 'capital-ship
      :team 'blue
      :position '(-60299.996960000004 17850.002043999993 53980.0)
      :rotation '(0.0 49.99999999999981 2.976433766769352e-15))
    (entity
      :id 'capitalship-red
      :archetype 'capital-ship
      :team 'red
      :position '(119430.00304 -200679.99795599998 53980.0)
      :rotation '(1.486428370026648e-14 -90.00000000000011 2.601840203173671e-14))
    (entity
      :id 'capitalship-red2
      :archetype 'capital-ship
      :team 'red
      :position '(119430.00303955744 -130399.99795578144 53979.99999999982)
      :rotation '(2.5444437451707945e-14 -89.99999999999996 2.544443745170794e-14))
    (entity
      :id 'capitalship-red3
      :archetype 'capital-ship
      :team 'red
      :position '(119430.00304 -287339.99795600004 53980.0)
      :rotation '(1.2528939636738173e-14 -89.99999999999999 2.5444437451707948e-14))
    (entity
      :id 'playership
      :archetype 'player-fighter
      :team 'blue
      :position '(-20220.0 -12270.0 55330.00000000001)
      :rotation '(4.7616320392269715e-14 -40.000000000001016 3.478198058140418e-14))
    (entity
      :id 'staticturret-blue
      :archetype 'static-turret
      :team 'blue
      :position '(-38360.0 35020.0 54710.0))
    (entity
      :id 'staticturret-blue10
      :archetype 'static-turret
      :team 'blue
      :position '(-37520.0 36420.0 52140.0))
    (entity
      :id 'staticturret-blue11
      :archetype 'static-turret
      :team 'blue
      :position '(-36420.0 37610.0 52140.0))
    (entity
      :id 'staticturret-blue12
      :archetype 'static-turret
      :team 'blue
      :position '(-39460.0 33830.0 52140.0))
    (entity
      :id 'staticturret-blue13
      :archetype 'static-turret
      :team 'blue
      :position '(-38360.0 35020.0 52140.0))
    (entity
      :id 'staticturret-blue14
      :archetype 'static-turret
      :team 'blue
      :position '(-71770.0 -6440.0 57120.0))
    (entity
      :id 'staticturret-blue15
      :archetype 'static-turret
      :team 'blue
      :position '(-72870.0 -7630.0 57120.0))
    (entity
      :id 'staticturret-blue16
      :archetype 'static-turret
      :team 'blue
      :position '(-75010.0 -10320.0 57120.0))
    (entity
      :id 'staticturret-blue17
      :archetype 'static-turret
      :team 'blue
      :position '(-73910.0 -9130.0 57120.0))
    (entity
      :id 'staticturret-blue18
      :archetype 'static-turret
      :team 'blue
      :position '(-76950.0 -12910.0 57120.0))
    (entity
      :id 'staticturret-blue19
      :archetype 'static-turret
      :team 'blue
      :position '(-75850.0 -11720.0 57120.0))
    (entity
      :id 'staticturret-blue2
      :archetype 'static-turret
      :team 'blue
      :position '(-72870.0 -7630.0 52140.0))
    (entity
      :id 'staticturret-blue20
      :archetype 'static-turret
      :team 'blue
      :position '(-35380.0 39110.0 57120.0))
    (entity
      :id 'staticturret-blue21
      :archetype 'static-turret
      :team 'blue
      :position '(-34280.0 40300.0 57120.0))
    (entity
      :id 'staticturret-blue22
      :archetype 'static-turret
      :team 'blue
      :position '(-37520.0 36420.0 57120.0))
    (entity
      :id 'staticturret-blue23
      :archetype 'static-turret
      :team 'blue
      :position '(-36420.0 37610.0 57120.0))
    (entity
      :id 'staticturret-blue24
      :archetype 'static-turret
      :team 'blue
      :position '(-39460.0 33830.0 57120.0))
    (entity
      :id 'staticturret-blue25
      :archetype 'static-turret
      :team 'blue
      :position '(-38360.0 35020.0 57120.0))
    (entity
      :id 'staticturret-blue26
      :archetype 'static-turret
      :team 'blue
      :position '(-71770.0 -6440.0 54710.0))
    (entity
      :id 'staticturret-blue27
      :archetype 'static-turret
      :team 'blue
      :position '(-72870.0 -7630.0 54710.0))
    (entity
      :id 'staticturret-blue28
      :archetype 'static-turret
      :team 'blue
      :position '(-75010.0 -10320.0 54710.0))
    (entity
      :id 'staticturret-blue29
      :archetype 'static-turret
      :team 'blue
      :position '(-73910.0 -9130.0 54710.0))
    (entity
      :id 'staticturret-blue3
      :archetype 'static-turret
      :team 'blue
      :position '(-71770.0 -6440.0 52140.0))
    (entity
      :id 'staticturret-blue30
      :archetype 'static-turret
      :team 'blue
      :position '(-76950.0 -12910.0 54710.0))
    (entity
      :id 'staticturret-blue31
      :archetype 'static-turret
      :team 'blue
      :position '(-75850.0 -11720.0 54710.0))
    (entity
      :id 'staticturret-blue32
      :archetype 'static-turret
      :team 'blue
      :position '(-35380.0 39110.0 54710.0))
    (entity
      :id 'staticturret-blue33
      :archetype 'static-turret
      :team 'blue
      :position '(-34280.0 40300.0 54710.0))
    (entity
      :id 'staticturret-blue34
      :archetype 'static-turret
      :team 'blue
      :position '(-37520.0 36420.0 54710.0))
    (entity
      :id 'staticturret-blue35
      :archetype 'static-turret
      :team 'blue
      :position '(-36420.0 37610.0 54710.0))
    (entity
      :id 'staticturret-blue36
      :archetype 'static-turret
      :team 'blue
      :position '(-39460.0 33830.0 54710.0))
    (entity
      :id 'staticturret-blue4
      :archetype 'static-turret
      :team 'blue
      :position '(-75010.0 -10320.0 52140.0))
    (entity
      :id 'staticturret-blue5
      :archetype 'static-turret
      :team 'blue
      :position '(-73910.0 -9130.0 52140.0))
    (entity
      :id 'staticturret-blue6
      :archetype 'static-turret
      :team 'blue
      :position '(-76950.0 -12910.0 52140.0))
    (entity
      :id 'staticturret-blue7
      :archetype 'static-turret
      :team 'blue
      :position '(-75850.0 -11720.0 52140.0))
    (entity
      :id 'staticturret-blue8
      :archetype 'static-turret
      :team 'blue
      :position '(-35380.0 39110.0 52140.0))
    (entity
      :id 'staticturret-blue9
      :archetype 'static-turret
      :team 'blue
      :position '(-34280.0 40300.0 52140.0)))))
