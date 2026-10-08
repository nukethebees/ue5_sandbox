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

  (define (make-capitals team center-x center-y center-z yaw)
    (map
      (lambda (slot)
        (let ((offset-x (car slot))
              (offset-y (cadr slot))
              (offset-z (caddr slot)))
          (entity :archetype 'capital-ship :team team
            :position (list
              (+ center-x offset-x)
              (+ center-y offset-y)
              (+ center-z offset-z))
            :rotation (list 0 yaw 0))))
      capital-slots))

  (define (make-turret-clusters team center-x center-y center-z yaw)
    (apply append
      (map
        (lambda (cluster)
          (let ((cluster-x (+ center-x (car cluster)))
                (cluster-y (+ center-y (cadr cluster)))
                (cluster-z (+ center-z (caddr cluster))))
            (map
              (lambda (offset)
                (let ((offset-x (car offset))
                      (offset-y (cadr offset))
                      (offset-z (caddr offset)))
                  (entity :archetype 'static-turret :team team
                    :position (list
                      (+ cluster-x offset-x)
                      (+ cluster-y offset-y)
                      (+ cluster-z offset-z))
                    :rotation (list 0 yaw 0))))
              turret-offsets)))
        turret-clusters)))

  (define (make-faction team center-x center-y center-z yaw)
    (append
      (make-capitals team center-x center-y center-z yaw)
      (make-turret-clusters team center-x center-y center-z yaw)))

  (define armada-entities
    (append
      (make-faction 'white -300000 -720000 -12000 16)
      (make-faction 'red 590000 -470000 15000 77)
      (make-faction 'green 750000 250000 -18000 140)
      (make-faction 'blue 100000 780000 10000 -159)
      (make-faction 'orange -680000 500000 -15000 20)
      (make-faction 'yellow -800000 -300000 18000 -34)))

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
