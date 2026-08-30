(begin
  (define ship-name (sbx-uppercase "wayward scheme"))
  (define velocity '(12 16 0))
  (define speed-squared (apply sbx-vector-length-squared velocity))
  (list 'ship ship-name 'velocity velocity 'speed-squared speed-squared))
