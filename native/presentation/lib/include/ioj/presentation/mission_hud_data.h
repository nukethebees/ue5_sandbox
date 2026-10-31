#pragma once

#include <cstdint>
#include <string>

namespace ioj::presentation {
struct MissionHudData {
    auto operator==(MissionHudData const&) const -> bool = default;

    std::string active_titles;
    std::int32_t active{};
    std::int32_t completed{};
    std::int32_t inactive{};
    double elapsed_seconds{};
};
}
