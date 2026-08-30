(begin
  (define values '(3 5 8 13 21))
  (define total (apply + values))
  (list 'values values 'total total 'average (/ total (length values))))
