; Colours and material references remain on the Unreal starfield actor.
(starfield
  :generation (generation :star-count 10000 :random-seed 1337)
  :star (star
    :scale 1
    :size-multiplier 1
    :global-brightness 8
    :colour-variation-strength 0.35
    :parallax-strength 0.01)
  :galactic-band (galactic-band :strength 0.65 :width-degrees 15)
  :stellar-cluster (stellar-cluster :strength 0.12 :width-degrees 4)
  :dust-lane (dust-lane :strength 0.9 :width-degrees 5 :irregularity 0.7)
  :bright-star (bright-star
    :fraction 0.01
    :size-multiplier 1.75
    :brightness-multiplier 2
    :shape-strength 0.25)
  :twinkle (twinkle :strength 0.1 :speed 0.35)
  :galactic-haze (galactic-haze :strength 0.15 :width-degrees 18)
  :galactic-core (galactic-core :strength 0.65 :width-degrees 32)
  :nebular-knot (nebular-knot :strength 0.18 :size-degrees 9))
