(export preferences)

; Omit resolution to use the desktop size.
(define preferences
  (user-settings
    :window-mode 'borderless
    :vsync #f
    :frame-rate-limit 0
    :resolution-scale 100
    :view-distance-quality 'epic
    :aa-method 'off
    :aa-quality 'epic
    :shadow-quality 'epic
    :global-illumination-quality 'epic
    :reflections-quality 'epic
    :post-processing-quality 'epic
    :texture-quality 'epic
    :effects-quality 'epic
    :shading-quality 'epic
    :bloom #t
    :motion-blur #f
    :master-volume 1
    :music-volume 1
    :sfx-volume 1
    :ui-volume 1
    :mouse-turn-sensitivity 0.25
    :gamepad-turn-sensitivity 1
    :gamepad-turn-dead-zone 0.25
    :invert-mouse-pitch #f
    :invert-mouse-yaw #f
    :invert-gamepad-pitch #f
    :invert-gamepad-yaw #f))
