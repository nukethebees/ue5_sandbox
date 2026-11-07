#pragma once

namespace mesh_gen {

struct Rotatorf {
    float pitch{};
    float yaw{};
    float roll{};

    [[nodiscard]] auto operator==(Rotatorf const&) const -> bool = default;
};

}
