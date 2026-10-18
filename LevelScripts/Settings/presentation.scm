; Numerical presentation tuning; colours, assets and scene bindings remain in Unreal.
(presentation
  :player-ship (settings-section 'player-ship
    :boost-effect-colour-intensity 75
    :barrel-roll (settings-section 'barrel-roll
      :roll-speed 1080
      :roll-duration 0.66
      :roll-cooldown 0.33))
  :laser-projectiles (settings-section 'laser-projectiles
    :min-cull-distance 0
    :max-cull-distance 50000)
  :capital-ships (settings-section 'capital-ships
    :n-small-explosions 6
    :time-between-explosions 0.1
    :min-small-explosion-range '(0 0 0)
    :max-small-explosion-range '(1 1 1)
    :large-explosion-delay 0
    :proxy-arrow-size 5)
  :turrets (settings-section 'turrets
    :death-effect-scale 1)
  :entity-overlay (settings-section 'entity-overlay
    :enabled #t
    :maximum-range 1000000
    :bar-size-pixels '(64 8)
    :screen-offset-pixels '(0 -24)
    :minimum-bar-scale 0.5
    :maximum-bar-scale 2
    :inset-pixels 1
    :maximum-inset-height-ratio 0.4
    :objective-frame-pixels 3
    :objective-bar-height-scale 1.4
    :screen-edge-padding-pixels 12
    :soft-target (settings-section 'soft-target
      :acquisition-radius-pixels 72
      :retention-radius-pixels 96
      :centre-tie-radius-pixels 4
      :switch-improvement-ratio 0.75
      :range-transition-start-multiplier 4
      :minimum-radius-pixels 28
      :maximum-radius-pixels 96
      :bounds-padding-pixels 10
      :bracket-start-radius-multiplier 2.5
      :opacity 0.7
      :pulse-opacity-boost 0.08
      :pulse-duration 0.15
      :fade-out-duration 0.15))
  :radar (settings-section 'radar
    :enabled #t
    :combat-range 100000
    :tactical-range 400000
    :maximum-range 2000000
    :combat-display-radius 0.45
    :grid-opacity 0.55
    :core-cell-radius 0.18
    :combat-cell-radius 0.075
    :tactical-cell-radius 0.045
    :strategic-cell-radius 0.025
    :glyph-size-scale 1
    :objective-size-multiplier 1.2
    :objective-ring-padding-pixels 3
    :objective-ring-thickness-pixels 1.25))

