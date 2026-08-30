local ship_name = sbx_uppercase("wayward lua")
local velocity = {12, 16, 0}
local speed_squared = sbx_vector_length_squared(velocity[1], velocity[2], velocity[3])

return string.format(
    "ship=%q, velocity={%d,%d,%d}, speed_squared=%.1f",
    ship_name,
    velocity[1],
    velocity[2],
    velocity[3],
    speed_squared)
