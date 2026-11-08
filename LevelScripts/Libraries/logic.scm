(define (destroy-all ids)
  (apply all-of (map entity-destroyed ids)))

(define (destroy-any ids)
  (apply any-of (map entity-destroyed ids)))

(define (level-delay seconds)
  (let ((clock (timer)))
    (initially (start-timer clock seconds))
    (timer-elapsed clock)))

(define (win-race team opponent success failure)
  (let ((decision (latch :when (any-of success failure) :value success)))
    (on :when (latched-true decision) :do (win team))
    (on :when (latched-false decision) :do (win opponent))))
