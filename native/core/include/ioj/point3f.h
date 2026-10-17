#pragma once

namespace ioj {
struct Point3f {
    float x{};
    float y{};
    float z{};

    auto operator==(Point3f const&) const -> bool = default;
};
}
