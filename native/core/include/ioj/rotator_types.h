#pragma once

namespace ioj {
struct Rotator3f {
    float pitch{};
    float yaw{};
    float roll{};

    auto operator==(Rotator3f const&) const -> bool = default;
};
} // namespace ioj
