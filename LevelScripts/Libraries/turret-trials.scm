(load-script "Libraries/formations.scm")

(define* (turret-trial number description player-position turret-positions
                      (time-limit #f) (par-time #f))
  (define (trial-id index)
    (string->symbol (format #f "turret-trial-~A" index)))

  (apply level
    (append
      (list :id (trial-id number)
            :title (format #f "Turret Trial ~A" number)
            :description description
            :player 'player
            :mission (apply mission
              (append
                (list :mode (if time-limit 'kill-enemies-within-time 'kill-enemies)
                      :heroes '(player) :must-survive '(player))
                (if time-limit (list :time-limit time-limit) '())))
            :entities (cons
              (entity :id 'player :archetype 'player-fighter :team 'blue
                :position player-position :rotation '(0 90 0))
              (entities-at 'static-turret 'red '(0 -90 0) turret-positions)))
      (if (> number 0)
          (list :unlock (list (level-completed (trial-id (- number 1)))))
          '())
      (if par-time (list :par-time par-time) '()))))
