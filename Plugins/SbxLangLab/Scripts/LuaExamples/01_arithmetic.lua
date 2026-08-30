local values = {3, 5, 8, 13, 21}
local total = 0

for _, value in ipairs(values) do
    total = total + value
end

return string.format("values={3,5,8,13,21}, total=%d, average=%.1f", total, total / #values)
