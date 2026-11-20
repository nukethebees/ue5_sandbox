(export entity-geometry)
(define entity-geometry (entity-geometry
  :player-ship (mesh-bounds
    :centre '(-96.9977035522461 -5.340576171875e-05 -5.7220458984375e-06)
    :half-extents '(173.01498413085938 185.37841796875 55.37841033935547))
  :capital-ship (mesh-bounds
    :centre '(4786.3125 0.000244140625 -1687.3070068359375)
    :half-extents '(8918.416015625 5000.00048828125 5077.6611328125))
  :fighter (mesh-bounds
    :centre '(0.0 0.0 0.0)
    :half-extents '(1720.65966796875 1021.2993774414062 438.31573486328125))
  :turret (mesh-bounds
    :centre '(0.0 0.0 567.5825805664062)
    :half-extents '(335.64892578125 335.64892578125 684.3348388671875))
  :tube-spinner (mesh-bounds
    :centre '(0.0 0.0 250.0)
    :half-extents '(50.0 50.0 250.0))
  :fighter-launch-slots (list
    (attachment-transform
      :location '(-300.0 7000.0 -2900.0)
      :rotation '(0.0 -0.0 0.7071067811865476 0.7071067811865477)
      :scale '(1.0 1.0 1.0))
    (attachment-transform
      :location '(3300.0 7000.0 -2900.0)
      :rotation '(0.0 -0.0 0.7071067811865476 0.7071067811865477)
      :scale '(1.0 1.0 1.0))
    (attachment-transform
      :location '(6900.0 7000.0 -2900.0)
      :rotation '(0.0 -0.0 0.7071067811865476 0.7071067811865477)
      :scale '(1.0 1.0 1.0))
    (attachment-transform
      :location '(-300.0 7000.0 700.0)
      :rotation '(0.0 -0.0 0.7071067811865476 0.7071067811865477)
      :scale '(1.0 1.0 1.0))
    (attachment-transform
      :location '(3300.0 7000.0 700.0)
      :rotation '(0.0 -0.0 0.7071067811865476 0.7071067811865477)
      :scale '(1.0 1.0 1.0))
    (attachment-transform
      :location '(6900.0 7000.0 700.0)
      :rotation '(0.0 -0.0 0.7071067811865476 0.7071067811865477)
      :scale '(1.0 1.0 1.0)))
  :fighter-fire-point-distance 1807.2945556640625
  :turret-fire-point '(0.0 0.0 1560.0)
  :spinner-fire-points (list
    (attachment-transform
      :location '(100.0 0.0 0.0)
      :rotation '(0.0 0.0 0.0 1.0)
      :scale '(1.0 1.0 1.0))
    (attachment-transform
      :location '(50.0 70.0 150.0)
      :rotation '(0.0 -0.0 0.45758158085794043 0.8891676427196099)
      :scale '(1.0 1.0 1.0))
    (attachment-transform
      :location '(-50.0 80.0 240.0)
      :rotation '(0.0 -0.0 0.8746424812468178 0.4847685323929451)
      :scale '(1.0 1.0 1.0))
    (attachment-transform
      :location '(-100.0 0.0 330.0)
      :rotation '(0.0 -0.0 0.9999999999999999 6.123233995736766e-17)
      :scale '(1.0 1.0 1.0))
    (attachment-transform
      :location '(-70.0 -80.0 420.0)
      :rotation '(-0.0 0.0 -0.9106329139308873 0.41321628243057046)
      :scale '(1.0 1.0 1.0))
    (attachment-transform
      :location '(0.0 -80.0 480.0)
      :rotation '(-0.0 0.0 -0.7071067811865476 0.7071067811865477)
      :scale '(1.0 1.0 1.0))
    (attachment-transform
      :location '(80.0 -80.0 420.0)
      :rotation '(-0.0 0.0 -0.3826834323650898 0.9238795325112866)
      :scale '(1.0 1.0 1.0))
    (attachment-transform
      :location '(100.0 -50.0 340.0)
      :rotation '(-0.0 0.0 -0.2297529205473611 0.9732489894677303)
      :scale '(1.0 1.0 1.0))
    (attachment-transform
      :location '(110.0 40.0 290.0)
      :rotation '(0.0 -0.0 0.17350299206578973 0.9848333421164306)
      :scale '(1.0 1.0 1.0)))
  :player-body (attachment-transform
    :location '(0.0 0.0 0.0)
    :rotation '(0.0 0.0 0.0 1.0)
    :scale '(1.0 1.0 1.0))
  :player-left-socket (attachment-transform
    :location '(200.0 -129.9999999999983 0.0)
    :rotation '(3.0750422794220998e-15 -3.7748949743843234e-08 8.146033980693785e-08 0.9999999999999958)
    :scale '(0.9999999776482583 0.9999999776482587 0.9999999776482585))
  :player-middle-socket (attachment-transform
    :location '(200.0 0.0 0.0)
    :rotation '(3.0750422794220998e-15 -3.7748949743843234e-08 8.146033980693785e-08 0.9999999999999958)
    :scale '(0.9999999776482583 0.9999999776482587 0.9999999776482585))
  :player-right-socket (attachment-transform
    :location '(200.0 139.99999999999815 0.0)
    :rotation '(3.0750422794220998e-15 -3.7748949743843234e-08 8.146033980693785e-08 0.9999999999999958)
    :scale '(0.9999999776482583 0.9999999776482587 0.9999999776482585))))
