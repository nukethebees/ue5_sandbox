(level
  :id 'defend-capital
  :title "defend_capital"
  :description ""
  :par-time :none
  :unlock :none
  :player 'playership
  :camera :none
  :settings (settings
    :tick (tick
      :tick-rate 60.0
      :time-scale 1.0)
    :player-ship (player-ship
      :thrust-energy-max 1.0
      :pitch-angle-max 30.0
      :pitch-speed 3.0
      :yaw-angle-max 30.0
      :yaw-speed 3.0
      :turn-bank-angle-max 30.0
      :turn-bank-speed 2.0
      :laser (laser
        :damage 50
        :projectile-speed 40000.0
        :max-distance 30000.0
        :fire-cooldown 0.15000000596046448)
      :health 1000
      :laser-mode 'single
      :laser-fire-rate 'full-auto)
    :laser-projectiles (laser-projectiles
      :n-preallocated-instances 50000)
    :capital-ships (capital-ships
      :spawn-delay 5.0
      :max-health 5000)
    :fighters (fighters
      :max-live-fighters 2000
      :fire-dot-product-threshold 0.949999988079071
      :attack-distance-band (attack-distance-band
        :minimum-ratio 0.4000000059604645
        :desired-ratio 0.5
        :maximum-ratio 0.6000000238418579)
      :speed 4500.0
      :turn-speed-unitless 1.0
      :avoidance-clear-update-frequency 2.0
      :avoidance-update-frequency 5.0
      :avoidance-active-update-frequency 12.0
      :avoidance-immediate-update-frequency 30.0
      :avoidance-lookahead-time 1.0
      :avoidance-clearance-buffer 100.0
      :separation-radius 2000.0
      :separation-strength 1.0
      :steering-memory-duration 0.75
      :dense-traffic-neighbour-threshold 4
      :laser (laser
        :damage 10
        :projectile-speed 10000.0
        :max-distance 40000.0
        :fire-cooldown 0.33000001311302185)
      :health 50
      :attack-retry-cooldown 0.15000000596046448
      :los-check-buffer 100.0)
    :turrets (turrets
      :detection-radius 50000.0
      :target-refresh-frequency 5.0
      :laser (laser
        :damage 5
        :projectile-speed 15000.0
        :max-distance 40000.0
        :fire-cooldown 0.33000001311302185)
      :max-health 20)
    :tube-spinners (tube-spinners
      :yaw-rotation-speed-degrees 66.0
      :laser (laser
        :damage 2
        :projectile-speed 10000.0
        :max-distance 10000.0
        :fire-cooldown 0.30000001192092896))
    :collision-grid (collision-grid
      :damage-per-overlap-detection 50
      :level-size '(2e+06 2e+06 1e+05)
      :cell-size '(5000.0 5000.0 20000.0))
    :hud (hud
      :player-status-update-period 0.25
      :entity-count-update-period 0.25)
    :presentation (presentation
      :player-ship (player-ship-presentation
        :boost-effect-colour-intensity 75.0)
      :laser-projectiles (laser-projectiles-presentation
        :min-cull-distance 0.0
        :max-cull-distance 50000.0)
      :capital-ships (capital-ships-presentation
        :n-small-explosions 6
        :time-between-explosions 0.10000000149011612
        :min-small-explosion-range '(0.0 0.0 0.0)
        :max-small-explosion-range '(1.0 1.0 1.0)
        :large-explosion-delay 0.0
        :proxy-arrow-size 5.0)
      :turrets (turrets-presentation
        :death-effect-scale 1.0)
      :entity-overlay (entity-overlay
        :enabled #t
        :maximum-range 1e+06
        :bar-size-pixels '(64.0 8.0)
        :screen-offset-pixels '(0.0 -24.0)
        :minimum-bar-scale 0.5
        :maximum-bar-scale 2.0
        :inset-pixels 1.0
        :maximum-inset-height-ratio 0.4000000059604645
        :objective-frame-pixels 3.0
        :objective-bar-height-scale 1.399999976158142
        :screen-edge-padding-pixels 12.0
        :soft-target (soft-target
          :acquisition-radius-pixels 72.0
          :retention-radius-pixels 96.0
          :centre-tie-radius-pixels 4.0
          :switch-improvement-ratio 0.75
          :range-transition-start-multiplier 4.0
          :minimum-radius-pixels 28.0
          :maximum-radius-pixels 96.0
          :bounds-padding-pixels 10.0
          :bracket-start-radius-multiplier 2.5
          :opacity 0.699999988079071
          :pulse-opacity-boost 0.07999999821186066
          :pulse-duration 0.15000000596046448
          :fade-out-duration 0.15000000596046448))
      :radar (radar
        :enabled #t
        :combat-range 1e+05
        :tactical-range 4e+05
        :maximum-range 2e+06
        :combat-display-radius 0.44999998807907104
        :grid-opacity 0.550000011920929
        :core-cell-radius 0.18000000715255737
        :combat-cell-radius 0.07500000298023224
        :tactical-cell-radius 0.04500000178813934
        :strategic-cell-radius 0.02500000037252903
        :glyph-size-scale 1.0
        :objective-size-multiplier 1.2000000476837158
        :objective-ring-padding-pixels 3.0
        :objective-ring-thickness-pixels 1.25))
    :starfield (starfield
      :generation (generation
        :star-count 10000
        :random-seed 1337)
      :star (star
        :scale 1.0
        :size-multiplier 1.0
        :global-brightness 8.0
        :colour-variation-strength 0.3499999940395355
        :parallax-strength 0.009999999776482582)
      :galactic-band (galactic-band
        :strength 0.6499999761581421
        :width-degrees 15.0)
      :stellar-cluster (stellar-cluster
        :strength 0.11999999731779099
        :width-degrees 4.0)
      :dust-lane (dust-lane
        :strength 0.8999999761581421
        :width-degrees 5.0
        :irregularity 0.699999988079071)
      :bright-star (bright-star
        :fraction 0.009999999776482582
        :size-multiplier 1.75
        :brightness-multiplier 2.0
        :shape-strength 0.25)
      :twinkle (twinkle
        :strength 0.10000000149011612
        :speed 0.3499999940395355)
      :galactic-haze (galactic-haze
        :strength 0.15000000596046448
        :width-degrees 18.0)
      :galactic-core (galactic-core
        :strength 0.6499999761581421
        :width-degrees 32.0)
      :nebular-knot (nebular-knot
        :strength 0.18000000715255737
        :size-degrees 9.0)))
  :entities (list
    (entity
      :id 'capitalship-blue
      :archetype 'capital-ship
      :team 'blue
      :position '(-60299.996960000004 17850.002043999993 53980.0)
      :rotation '(0.0 49.99999999999981 2.976433766769352e-15)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'capitalship-red
      :archetype 'capital-ship
      :team 'red
      :position '(119430.00304 -200679.99795599998 53980.0)
      :rotation '(1.486428370026648e-14 -90.00000000000011 2.601840203173671e-14)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'capitalship-red2
      :archetype 'capital-ship
      :team 'red
      :position '(119430.00303955744 -130399.99795578144 53979.99999999982)
      :rotation '(2.5444437451707945e-14 -89.99999999999996 2.544443745170794e-14)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'capitalship-red3
      :archetype 'capital-ship
      :team 'red
      :position '(119430.00304 -287339.99795600004 53980.0)
      :rotation '(1.2528939636738173e-14 -89.99999999999999 2.5444437451707948e-14)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'playership
      :archetype 'player-fighter
      :team 'blue
      :position '(-20220.0 -12270.0 55330.00000000001)
      :rotation '(4.7616320392269715e-14 -40.000000000001016 3.478198058140418e-14)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue
      :archetype 'static-turret
      :team 'blue
      :position '(-38360.0 35020.0 54710.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue10
      :archetype 'static-turret
      :team 'blue
      :position '(-37520.0 36420.0 52140.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue11
      :archetype 'static-turret
      :team 'blue
      :position '(-36420.0 37610.0 52140.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue12
      :archetype 'static-turret
      :team 'blue
      :position '(-39460.0 33830.0 52140.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue13
      :archetype 'static-turret
      :team 'blue
      :position '(-38360.0 35020.0 52140.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue14
      :archetype 'static-turret
      :team 'blue
      :position '(-71770.0 -6440.0 57120.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue15
      :archetype 'static-turret
      :team 'blue
      :position '(-72870.0 -7630.0 57120.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue16
      :archetype 'static-turret
      :team 'blue
      :position '(-75010.0 -10320.0 57120.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue17
      :archetype 'static-turret
      :team 'blue
      :position '(-73910.0 -9130.0 57120.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue18
      :archetype 'static-turret
      :team 'blue
      :position '(-76950.0 -12910.0 57120.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue19
      :archetype 'static-turret
      :team 'blue
      :position '(-75850.0 -11720.0 57120.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue2
      :archetype 'static-turret
      :team 'blue
      :position '(-72870.0 -7630.0 52140.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue20
      :archetype 'static-turret
      :team 'blue
      :position '(-35380.0 39110.0 57120.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue21
      :archetype 'static-turret
      :team 'blue
      :position '(-34280.0 40300.0 57120.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue22
      :archetype 'static-turret
      :team 'blue
      :position '(-37520.0 36420.0 57120.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue23
      :archetype 'static-turret
      :team 'blue
      :position '(-36420.0 37610.0 57120.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue24
      :archetype 'static-turret
      :team 'blue
      :position '(-39460.0 33830.0 57120.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue25
      :archetype 'static-turret
      :team 'blue
      :position '(-38360.0 35020.0 57120.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue26
      :archetype 'static-turret
      :team 'blue
      :position '(-71770.0 -6440.0 54710.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue27
      :archetype 'static-turret
      :team 'blue
      :position '(-72870.0 -7630.0 54710.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue28
      :archetype 'static-turret
      :team 'blue
      :position '(-75010.0 -10320.0 54710.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue29
      :archetype 'static-turret
      :team 'blue
      :position '(-73910.0 -9130.0 54710.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue3
      :archetype 'static-turret
      :team 'blue
      :position '(-71770.0 -6440.0 52140.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue30
      :archetype 'static-turret
      :team 'blue
      :position '(-76950.0 -12910.0 54710.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue31
      :archetype 'static-turret
      :team 'blue
      :position '(-75850.0 -11720.0 54710.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue32
      :archetype 'static-turret
      :team 'blue
      :position '(-35380.0 39110.0 54710.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue33
      :archetype 'static-turret
      :team 'blue
      :position '(-34280.0 40300.0 54710.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue34
      :archetype 'static-turret
      :team 'blue
      :position '(-37520.0 36420.0 54710.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue35
      :archetype 'static-turret
      :team 'blue
      :position '(-36420.0 37610.0 54710.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue36
      :archetype 'static-turret
      :team 'blue
      :position '(-39460.0 33830.0 54710.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue4
      :archetype 'static-turret
      :team 'blue
      :position '(-75010.0 -10320.0 52140.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue5
      :archetype 'static-turret
      :team 'blue
      :position '(-73910.0 -9130.0 52140.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue6
      :archetype 'static-turret
      :team 'blue
      :position '(-76950.0 -12910.0 52140.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue7
      :archetype 'static-turret
      :team 'blue
      :position '(-75850.0 -11720.0 52140.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue8
      :archetype 'static-turret
      :team 'blue
      :position '(-35380.0 39110.0 52140.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)
    (entity
      :id 'staticturret-blue9
      :archetype 'static-turret
      :team 'blue
      :position '(-34280.0 40300.0 52140.0)
      :rotation '(0.0 0.0 0.0)
      :spawn-at 0.0
      :health :none
      :laser-damage :none
      :initial-spawn-delay :none
      :spawn-cooldown :none
      :initial-fire-point :none)))
