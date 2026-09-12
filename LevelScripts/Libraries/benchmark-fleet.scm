(define (benchmark-fleet-entities capital-count-per-team formation-columns)
  (define column-spacing 24000)
  (define row-spacing 24000)
  (define front-separation 240000)

  (define (make-indices count)
    (define (loop index result)
      (if (= index count)
        (reverse result)
        (loop (+ index 1) (cons index result))))
    (loop 0 '()))

  (define (capital-id team index)
    (string->symbol (format #f "~A-capital-~A" team index)))

  (define (make-capital team index formation-y row-direction yaw)
    (let ((column (modulo index formation-columns))
          (row (quotient index formation-columns)))
      (entity (capital-id team index) 'capital-ship team
        (position
          (* (- column (/ (- formation-columns 1) 2.0)) column-spacing)
          (+ formation-y (* row-direction row row-spacing))
          (* (- (modulo index 3) 1) 5000))
        (rotation 0 yaw 0))))

  (define (make-fleet team formation-y row-direction yaw)
    (map
      (lambda (index)
        (make-capital team index formation-y row-direction yaw))
      (make-indices capital-count-per-team)))

  (append
    (make-fleet 'blue (- (/ front-separation 2)) -1 90)
    (make-fleet 'red (/ front-separation 2) 1 -90)))
