; Retain DA_FT_soa_entities_LevelConfig tuning from the exported native reference.
; Tick, collision and HUD defaults retain the existing runtime defaults.
(settings-layers
  (settings
    :tick (tick :tick-rate 60 :time-scale 1)
    :player-ship (player-ship
      :thrust-energy-max 1
      :pitch-angle-max 30 :pitch-speed 3
      :yaw-angle-max 30 :yaw-speed 3
      :turn-bank-angle-max 30 :turn-bank-speed 2
      :laser (laser :damage 50 :projectile-speed 40000 :max-distance 30000 :fire-cooldown 0.15))
    :laser-projectiles (laser-projectiles :n-preallocated-instances 50000)
    :capital-ships (capital-ships :spawn-delay 5 :max-health 5000)
    :fighters (fighters
      :max-live-fighters 2000
      :fire-dot-product-threshold 0.95
      :speed 4500 :turn-speed-unitless 1
      :avoidance-clear-update-frequency 2
      :avoidance-update-frequency 5
      :avoidance-active-update-frequency 12
      :avoidance-immediate-update-frequency 30
      :avoidance-lookahead-time 1
      :avoidance-clearance-buffer 100
      :separation-radius 2000 :separation-strength 1
      :steering-memory-duration 0.75
      :dense-traffic-neighbour-threshold 4
      :health 50
      :attack-retry-cooldown 0.15
      :attack-engagement-threshold 5000
      :attack-reposition-frequency 10
      :attack-distance-band (attack-distance-band
        :minimum-ratio 0.4 :desired-ratio 0.5 :maximum-ratio 0.6)
      :arrival-distance 500 :los-check-buffer 100
      :awareness-radius 10000 :awareness-scan-frequency 6
      :minimum-opportunistic-intercept-deviation-dot-product 0.5
      :laser (laser :damage 10 :projectile-speed 10000 :max-distance 40000 :fire-cooldown 0.33))
    :turrets (turrets
      :detection-radius 50000 :target-refresh-frequency 5 :max-health 20
      :laser (laser :damage 5 :projectile-speed 15000 :max-distance 40000 :fire-cooldown 0.33))
    :tube-spinners (tube-spinners
      :yaw-rotation-speed-degrees 66
      :laser (laser :damage 2 :projectile-speed 10000 :max-distance 10000 :fire-cooldown 0.3))
    :collision-grid (collision-grid
      :level-size '(2000000 2000000 100000)
      :cell-size '(5000 5000 20000)
      :damage-per-overlap-detection 50)
    :hud (hud
      :player-status-update-period 0.25
      :entity-count-update-period 0.25
      :mission-status-update-period 0.25))
  (load-script "Settings/presentation.scm"))
