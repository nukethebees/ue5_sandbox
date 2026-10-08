(load-script "Libraries/formations.scm")

(level
  :id 'strike-range-clearance
  :title "Strike 01: Range Clearance"
  :description "Eliminate an isolated three-turret battery and return to operational control."

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
      '((-6000 15000 -1500)
        (0 15000 1500)
        (6000 15000 -1500)))))
