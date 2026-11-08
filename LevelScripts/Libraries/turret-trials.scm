(load-script "Libraries/logic.scm")
(load-script "Libraries/formations.scm")

(define* (turret-trial number description player-position turret-positions
                      (time-limit #f) (par-time #f))
  (define (trial-id index)
    (string->symbol (format #f "turret-trial-~A" index)))

  (apply level
    (append
      (list :settings (load-script "Settings/base.scm")
            :id (trial-id number)
            :title (format #f "Turret Trial ~A" number)
            :description description
            :player 'player
            :logic (logic
    (win-race 'blue 'red
              (entity-kills-reached
                '(player)
                (length turret-positions))
              (apply any-of
                (append
                  (list (entity-destroyed 'player))
                  (if time-limit (list (level-delay time-limit)) '())))))
            :entities (cons
              (entity :id 'player :archetype 'player-fighter :team 'blue
                :position player-position :rotation '(0 90 0))
              (entities-at 'static-turret 'red '(0 -90 0) turret-positions)))
      (if (> number 0)
          (list :unlock (list (level-completed (trial-id (- number 1)))))
          '())
      (if par-time (list :par-time par-time) '()))))
