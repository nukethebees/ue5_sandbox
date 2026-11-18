(load-script "Libraries/logic.scm")

(define scene (load-script "defend_capital/editor-config.scm"))

;; Keep the first wave clear of the carrier's world-aligned collision bounds.
(define (orient-launch-carrier ship)
  (if (eq? (entity-ref ship :id) 'capitalship-blue)
      (entity-with ship :rotation '(0 90 0))
      ship))

(level-with scene
  :entities (map orient-launch-carrier (level-ref scene :entities))
  :settings (settings-layers
    (load-script "Settings/base.scm")
    (settings
      :collision-grid (collision-grid
        :level-size '(2000000 2000000 500000)
        :cell-size '(5000 5000 20000))))

  :logic (logic
    (win-race 'blue 'red
      (entity-destroyed 'capitalship-red2)
      (any-of
        (entity-destroyed 'capitalship-blue)
        (level-delay 60)))))
