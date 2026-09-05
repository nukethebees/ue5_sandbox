(define capital-slots
  '((0 -35000 -45000 -6000)
    (1 35000 -45000 2000)
    (2 -35000 -15000 7000)
    (3 35000 -15000 -3000)
    (4 -35000 15000 -1000)
    (5 35000 15000 6000)
    (6 -35000 45000 3000)
    (7 35000 45000 -7000)))

(define turret-clusters
  '((0 -90000 -60000 -6000)
    (1 -30000 -70000 5000)
    (2 30000 -70000 -3000)
    (3 90000 -60000 7000)
    (4 -90000 -20000 2000)
    (5 90000 -20000 -6000)
    (6 -90000 20000 7000)
    (7 90000 20000 -1000)
    (8 -60000 65000 -4000)
    (9 60000 65000 4000)))

(define turret-offsets
  '((0 0 -3000 0)
    (1 -2600 1500 1000)
    (2 2600 1500 -1000)))

(define (entity-id team archetype index)
  (string->symbol (format #f "~A-~A-~A" team archetype index)))

(define (turret-id team cluster-index turret-index)
  (string->symbol (format #f "~A-turret-~A-~A" team cluster-index turret-index)))

(define (make-capitals team center-x center-y center-z yaw)
  (map
    (lambda (slot)
      (let ((index (car slot))
            (offset-x (cadr slot))
            (offset-y (caddr slot))
            (offset-z (cadddr slot)))
        (entity (entity-id team 'capital index) 'capital-ship team
          (position
            (+ center-x offset-x)
            (+ center-y offset-y)
            (+ center-z offset-z))
          (rotation 0 yaw 0))))
    capital-slots))

(define (make-turret-clusters team center-x center-y center-z yaw)
  (apply append
    (map
      (lambda (cluster)
        (let ((cluster-index (car cluster))
              (cluster-x (+ center-x (cadr cluster)))
              (cluster-y (+ center-y (caddr cluster)))
              (cluster-z (+ center-z (cadddr cluster))))
          (map
            (lambda (offset)
              (let ((turret-index (car offset))
                    (offset-x (cadr offset))
                    (offset-y (caddr offset))
                    (offset-z (cadddr offset)))
                (entity (turret-id team cluster-index turret-index) 'static-turret team
                  (position
                    (+ cluster-x offset-x)
                    (+ cluster-y offset-y)
                    (+ cluster-z offset-z))
                  (rotation 0 yaw 0))))
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
  (id 'six-faction-armada)
  (title "Six-Faction Armada")
  (description "A massive free-play battle with six widely separated fleets scattered across three dimensions.")

  (teams
    (team 'white)
    (team 'red)
    (team 'green)
    (team 'blue)
    (team 'orange)
    (team 'yellow))

  (player 'player)

  (apply entities
    (cons
      (entity 'player 'player-fighter 'blue
        (position 185000 830000 18000)
        (rotation 0 -159 0))
      armada-entities)))
