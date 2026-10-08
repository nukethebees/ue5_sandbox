(load-script "Libraries/formations.scm")

(let ()
  (define capital-slots
    '((-35000 -45000 -6000)
      (35000 -45000 2000)
      (-35000 -15000 7000)
      (35000 -15000 -3000)
      (-35000 15000 -1000)
      (35000 15000 6000)
      (-35000 45000 3000)
      (35000 45000 -7000)))

  (define turret-clusters
    '((-90000 -60000 -6000)
      (-30000 -70000 5000)
      (30000 -70000 -3000)
      (90000 -60000 7000)
      (-90000 -20000 2000)
      (90000 -20000 -6000)
      (-90000 20000 7000)
      (90000 20000 -1000)
      (-60000 65000 -4000)
      (60000 65000 4000)))

  (define turret-offsets
    '((0 -3000 0)
      (-2600 1500 1000)
      (2600 1500 -1000)))

  (define (make-faction team origin yaw)
    (append
      (entities-at 'capital-ship team (list 0 yaw 0)
        (translate-positions origin capital-slots))
      (apply append
        (map
          (lambda (center)
            (entities-at 'static-turret team (list 0 yaw 0)
              (translate-positions center turret-offsets)))
          (translate-positions origin turret-clusters)))))

  (define armada-entities
    (append
      (make-faction 'white '(-300000 -720000 -12000) 16)
      (make-faction 'red '(590000 -470000 15000) 77)
      (make-faction 'green '(750000 250000 -18000) 140)
      (make-faction 'blue '(100000 780000 10000) -159)
      (make-faction 'orange '(-680000 500000 -15000) 20)
      (make-faction 'yellow '(-800000 -300000 18000) -34)))

  (level
    :id 'six-faction-armada
    :title "Six-Faction Armada"
    :description "A massive free-play battle with six widely separated fleets scattered across three dimensions."

    :player 'player

    :entities (cons
        (entity :id 'player :archetype 'player-fighter :team 'blue
          :position '(185000 830000 18000)
          :rotation '(0 -159 0))
        armada-entities)))
