(level
  :id 'strike-layered-battery
  :title "Strike 02: Layered Battery"
  :description "Clear two mutually supporting turret clusters distributed across multiple elevations."

  :unlock (list
    (level-completed 'strike-range-clearance))

  :teams '(blue
    red)

  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :heroes '(player)
    :must-survive '(player))

  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -75000 1000)
      :rotation '(0 90 0))

    (entity :id 'turret-0 :archetype 'static-turret :team 'red
      :position '(-12000 10000 -2500)
      :rotation '(0 -90 0))

    (entity :id 'turret-1 :archetype 'static-turret :team 'red
      :position '(-8000 6000 2500)
      :rotation '(0 -90 0))

    (entity :id 'turret-2 :archetype 'static-turret :team 'red
      :position '(-4000 10000 2500)
      :rotation '(0 -90 0))

    (entity :id 'turret-3 :archetype 'static-turret :team 'red
      :position '(-8000 14000 -2500)
      :rotation '(0 -90 0))

    (entity :id 'turret-4 :archetype 'static-turret :team 'red
      :position '(4000 10000 -2500)
      :rotation '(0 -90 0))

    (entity :id 'turret-5 :archetype 'static-turret :team 'red
      :position '(8000 6000 -2500)
      :rotation '(0 -90 0))

    (entity :id 'turret-6 :archetype 'static-turret :team 'red
      :position '(12000 10000 2500)
      :rotation '(0 -90 0))

    (entity :id 'turret-7 :archetype 'static-turret :team 'red
      :position '(8000 14000 2500)
      :rotation '(0 -90 0))))
