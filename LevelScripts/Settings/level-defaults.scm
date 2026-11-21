(export game-settings)

(define game-settings
  (settings-layers
    (module-ref (import-module "Settings/simulation.scm") 'simulation-settings)
    (module-ref (import-module "Settings/presentation.scm") 'presentation-settings)
    (settings :entity-geometry
      (module-ref (import-module "Settings/entity-geometry.scm") 'entity-geometry))))
