#pragma once

#include <cstdint>

namespace ioj {
struct ScreenResolution {
    auto operator==(ScreenResolution const&) const -> bool = default;

    std::int32_t width{};
    std::int32_t height{};
};
}
