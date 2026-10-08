;; Return entities in position order so generated IDs and spawn order remain stable.
(define (entities-at archetype team rotation positions)
  (map
    (lambda (position)
      (entity :archetype archetype :team team
        :position position :rotation rotation))
    positions))

;; Keep IDs beside their positions for units referenced by cameras or missions.
(define (named-capitals team rotation slots)
  (map
    (lambda (slot)
      (entity :id (car slot) :archetype 'capital-ship :team team
        :position (cadr slot) :rotation rotation))
    slots))

(define (positions-on-line count origin step)
  (unless (and (integer? count) (>= count 0))
    (error 'formation-error "Line position count must be a non-negative integer."))
  (let loop ((index 0) (positions '()))
    (if (= index count)
        (reverse positions)
        (loop (+ index 1)
          (cons (list
                  (+ (car origin) (* index (car step)))
                  (+ (cadr origin) (* index (cadr step)))
                  (+ (caddr origin) (* index (caddr step))))
                positions)))))

(define (translate-positions origin positions)
  (map
    (lambda (position)
      (list (+ (car origin) (car position))
            (+ (cadr origin) (cadr position))
            (+ (caddr origin) (caddr position))))
    positions))
