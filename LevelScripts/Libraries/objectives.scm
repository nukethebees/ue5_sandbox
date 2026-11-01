;; These helpers expand to ordinary objective expressions.
(define (destroy-all ids)
  (apply all-of (map entity-destroyed ids)))

(define (destroy-any ids)
  (apply any-of (map entity-destroyed ids)))

(define (decisive-mission team opponent pass fail)
  (mission
    :objectives (list
      (objective :id 'primary
        :title "Primary objective"
        :team team
        :wins-level 'yes
        :pass pass
        :fail fail)
      (objective :id 'opposition
        :team opponent
        :wins-level 'yes
        :pass (objective-resolved 'primary 'failed)))))

;; Both sides observe terminal outcomes; no cross-objective expression sharing is required.
(define (reciprocal-objectives attack-id defend-id attacker defender success failure)
  (list
    (objective :id attack-id
      :team attacker
      :wins-level 'yes
      :pass success
      :fail failure)
    (objective :id defend-id
      :team defender
      :wins-level 'yes
      :pass (objective-resolved attack-id 'failed)
      :fail (objective-resolved attack-id 'passed))))
