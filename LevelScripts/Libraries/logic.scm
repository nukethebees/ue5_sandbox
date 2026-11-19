(export destroy-all destroy-any level-delay win-race)

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

;; Award victory on the deciding tick, preferring success on ties.
(define (win-race team opponent success failure)
  (on :when success :do (win team))
  (on :when (all-of failure (not-of success)) :do (win opponent)))
