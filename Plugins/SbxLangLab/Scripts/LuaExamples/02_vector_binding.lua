local x, y, z = 2, 3, 6
local length_squared = sbx_vector_length_squared(x, y, z)

return string.format("vector={%d,%d,%d}, length_squared=%.1f", x, y, z, length_squared)
