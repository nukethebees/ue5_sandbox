#pragma once

#include <cstdint>

namespace ioj::s7::detail {
struct LoadToken {
    std::int64_t value{};
    auto operator==(LoadToken const&) const -> bool = default;
};
}
