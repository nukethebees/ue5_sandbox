(begin
  (define x 2)
  (define y 3)
  (define z 6)
  (define length-squared (sbx-vector-length-squared x y z))
  (list 'vector (list x y z) 'length-squared length-squared))
