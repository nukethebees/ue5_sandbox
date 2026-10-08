(load-script "Libraries/formations.scm")

(level
  :id 'counteroffensive-decapitation
  :title "Counteroffensive 01: Decapitation"
  :description "Destroy an outnumbering enemy capital formation before its operating window closes."

  :unlock (list
    (level-completed 'defence-last-anchorage))

  :player 'player

  :mission (mission
    :mode 'kill-enemies-within-time
    :time-limit 210
    :kill-count 3
    :heroes '(player blue-capital-0 blue-capital-1)
    :must-survive '(player)
    :required-kills '(red-capital-0 red-capital-1 red-capital-2))

  :entities (append
    (list
      (entity :id 'player :archetype 'player-fighter :team 'blue
        :position '(0 -95000 8000)
        :rotation '(0 90 0)))

    (named-capitals 'blue '(0 90 0)
      '((blue-capital-0 (-25000 -50000 -4000))
        (blue-capital-1 (25000 -50000 4000))))

    (named-capitals 'red '(0 -90 0)
      '((red-capital-0 (-45000 70000 6000))
        (red-capital-1 (0 70000 0))
        (red-capital-2 (45000 70000 -6000))))))
