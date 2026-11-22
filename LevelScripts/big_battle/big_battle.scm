(export mission)

(define editor-config-module 
  (import-module "big_battle/editor-config.scm"))
(define level-defaults-module 
  (import-module "Settings/level-defaults.scm"))

(define scene 
  (module-ref editor-config-module 'scene))

(define game-settings 
  (module-ref level-defaults-module 'game-settings))

(define mission
  (copy-with scene
    :settings (merge-settings
      game-settings
      (level-ref scene :settings)
      (settings
        :collision-grid (collision-grid
          :level-size '(5000000 5000000 500000))))

    :logic (logic
      (define blue-flagship-destroyed 
        (entity-destroyed 'capitalship-blue))
      (define orange-flagship-destroyed 
        (entity-destroyed 'capitalship-blue3))
      (define green-flagship-destroyed 
        (entity-destroyed 'capitalship-green))
      (define red-flagship-destroyed 
        (entity-destroyed 'capitalship-red)))))
