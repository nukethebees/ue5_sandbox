(load-script "Libraries/formations.scm")

(level
  :id 'strike-layered-battery
  :title "Strike 02: Layered Battery"
  :description "Clear two mutually supporting turret clusters distributed across multiple elevations."

  :unlock (list
    (level-completed 'strike-range-clearance))

  :player 'player

  :mission (mission
    :mode 'kill-enemies
    :heroes '(player)
    :must-survive '(player))

  :entities (cons
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 -75000 1000)
      :rotation '(0 90 0))

    (entities-at 'static-turret 'red '(0 -90 0)
      '((-12000 10000 -2500)
        (-8000 6000 2500)
        (-4000 10000 2500)
        (-8000 14000 -2500)
        (4000 10000 -2500)
        (8000 6000 -2500)
        (12000 10000 2500)
        (8000 14000 2500)))))
