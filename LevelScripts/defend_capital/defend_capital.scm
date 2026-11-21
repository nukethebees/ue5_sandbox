(export mission)

(define logic-library (import-module "Libraries/logic.scm"))
(define level-delay (module-ref logic-library 'level-delay))
(define win-race (module-ref logic-library 'win-race))

(define scene (module-ref (import-module "defend_capital/editor-config.scm") 'scene))

(define mission
  (copy-with scene
    :settings (settings-layers
      (module-ref (import-module "Settings/level-defaults.scm") 'game-settings)
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
