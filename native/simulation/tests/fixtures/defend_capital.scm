(export mission)

(define logic-library (import-module "Libraries/logic.scm"))
(define level-delay (module-ref logic-library 'level-delay))
(define win-race (module-ref logic-library 'win-race))

(define scene (module-ref (import-module "defend_capital/editor-config.scm") 'scene))

;; Keep the first wave clear of the carrier's world-aligned collision bounds.
(define (orient-launch-carrier ship)
  (if (eq? (entity-ref ship :id) 'capitalship-blue)
      (copy-with ship :rotation '(0 90 0))
      ship))

(define mission
  (copy-with scene
    :entities (map orient-launch-carrier (level-ref scene :entities))
    :settings (settings-layers
      (module-ref (import-module "Settings/base.scm") 'game-settings)
      (level-ref scene :settings)
      (settings
        :collision-grid (collision-grid
          :level-size '(2000000 2000000 500000))))

    :logic (logic
      (win-race 'blue 'red
        (entity-destroyed 'capitalship-red2)
        (any-of
          (entity-destroyed 'capitalship-blue)
          (level-delay 60))))))
