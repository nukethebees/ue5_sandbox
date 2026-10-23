#pragma once

#include <cstdint>

struct FTarget {
    auto get(std::int32_t const offset = 0) const -> std::int32_t { return value + offset; }
    void set(std::int32_t const new_value) { value = new_value; }
    void clear() { value = 0; }

    std::int32_t value{};
};
