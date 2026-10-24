;; These helpers expand to ordinary objective expressions and team mappings.
(define (destroy-all ids)
  (apply all-of (map entity-destroyed ids)))

(define (destroy-any ids)
  (apply any-of (map entity-destroyed ids)))

(define (decisive-mission team pass fail)
  (mission
    :objectives (list
      (objective :id 'primary
        :title "Primary objective"
        :team team
        :pass pass
        :fail fail))
    :team-results (list
      (team-result :team team
        :when (objective-resolved 'primary 'passed)
        :result 'victory)
      (team-result :team team
        :when (objective-resolved 'primary 'failed)
        :result 'defeat))))

;; Both sides observe terminal outcomes; no cross-objective expression sharing is required.
(define (reciprocal-objectives attack-id defend-id attacker defender success failure)
  (list
    (objective :id attack-id
      :team attacker
      :pass success
      :fail failure)
    (objective :id defend-id
      :team defender
      :pass (objective-resolved attack-id 'failed)
      :fail (objective-resolved attack-id 'passed))))
