(export mission)
(define scene (module-ref (import-module "big_battle/editor-config.scm") 'scene))

(define mission
  (copy-with scene
    :settings (settings-layers
      (module-ref (import-module "Settings/level-defaults.scm") 'game-settings)
      (level-ref scene :settings)
      (settings
        :collision-grid (collision-grid
          :level-size '(2000000 2000000 500000))))))