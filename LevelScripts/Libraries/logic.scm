;; Require every listed entity to be destroyed.
(define (destroy-all ids)
  (apply all-of (map entity-destroyed ids)))

;; Require at least one listed entity to be destroyed.
(define (destroy-any ids)
  (apply any-of (map entity-destroyed ids)))

;; Test whether the specified simulation time has elapsed since level start.
(define (level-delay seconds)
  (let ((clock (timer)))
    (initially (start-timer clock seconds))
    (timer-elapsed clock)))

;; Latch the outcome and award victory, preferring success on ties.
(define (win-race team opponent success failure)
  (let ((decision (latch :when (any-of success failure) :value success)))
    (on 
      :when (latched-true decision) 
      :do (win team))
    (on 
      :when (latched-false decision) 
      :do (win opponent))))
