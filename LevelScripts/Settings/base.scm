; Retain DA_FT_soa_entities_LevelConfig tuning from the exported native reference.
; Tick, collision and HUD defaults retain the existing runtime defaults.
(settings
  :tick (tick
    :tick-rate 60
    :time-scale 1)
  :player-ship (player-ship
    :health 1000
    :laser-mode 'single
    :laser-fire-rate 'full-auto
    :thrust-energy-max 1
    :pitch-angle-max 30
    :pitch-speed 3
    :yaw-angle-max 30
    :yaw-speed 3
    :turn-bank-angle-max 30
    :turn-bank-speed 2
    :laser (laser
      :damage 50
      :projectile-speed 40000
      :max-distance 30000
      :fire-cooldown 0.15))
  :laser-projectiles (laser-projectiles
    :n-preallocated-instances 50000)
  :capital-ships (capital-ships
    :spawn-delay 5
    :max-health 5000)
  :fighters (fighters
    :max-live-fighters 2000
    :fire-dot-product-threshold 0.95
    :speed 4500
    :turn-speed-unitless 1
    :avoidance-clear-update-frequency 2
    :avoidance-update-frequency 5
    :avoidance-active-update-frequency 12
    :avoidance-immediate-update-frequency 30
    :avoidance-lookahead-time 1
    :avoidance-clearance-buffer 100
    :separation-radius 2000
    :separation-strength 1
    :steering-memory-duration 0.75
    :dense-traffic-neighbour-threshold 4
    :health 50
    :attack-retry-cooldown 0.15
    :attack-engagement-threshold 5000
    :attack-reposition-frequency 10
    :attack-distance-band (attack-distance-band
      :minimum-ratio 0.4
      :desired-ratio 0.5
      :maximum-ratio 0.6)
    :arrival-distance 500
    :los-check-buffer 100
    :awareness-radius 10000
    :awareness-scan-frequency 6
    :minimum-opportunistic-intercept-deviation-dot-product 0.5
    :laser (laser
      :damage 10
      :projectile-speed 10000
      :max-distance 40000
      :fire-cooldown 0.33))
  :turrets (turrets
    :detection-radius 50000
    :target-refresh-frequency 5
    :max-health 20
    :laser (laser
      :damage 5
      :projectile-speed 15000
      :max-distance 40000
      :fire-cooldown 0.33))
  :tube-spinners (tube-spinners
    :yaw-rotation-speed-degrees 66
    :laser (laser
      :damage 2
      :projectile-speed 10000
      :max-distance 10000
      :fire-cooldown 0.3))
  :collision-grid (collision-grid
    :level-size '(2000000 2000000 100000)
    :cell-size '(5000 5000 20000)
    :damage-per-overlap-detection 50)
  :hud (hud
    :player-status-update-period 0.25
    :entity-count-update-period 0.25
    :mission-status-update-period 0.25)

  :presentation (presentation
    :player-ship (settings-section 'player-ship
      :boost-effect-colour-intensity 75)
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

  :starfield (starfield
    :generation (generation
      :star-count 10000
      :random-seed 1337)
    :star (star
      :scale 1
      :size-multiplier 1
      :global-brightness 8
      :colour-variation-strength 0.35
      :parallax-strength 0.01)
    :galactic-band (galactic-band
      :strength 0.65
      :width-degrees 15)
    :stellar-cluster (stellar-cluster
      :strength 0.12
      :width-degrees 4)
    :dust-lane (dust-lane
      :strength 0.9
      :width-degrees 5
      :irregularity 0.7)
    :bright-star (bright-star
      :fraction 0.01
      :size-multiplier 1.75
      :brightness-multiplier 2
      :shape-strength 0.25)
    :twinkle (twinkle
      :strength 0.1
      :speed 0.35)
    :galactic-haze (galactic-haze
      :strength 0.15
      :width-degrees 18)
    :galactic-core (galactic-core
      :strength 0.65
      :width-degrees 32)
    :nebular-knot (nebular-knot
      :strength 0.18
      :size-degrees 9)))
