#pragma once

namespace ioj {
struct Point2f {
    float x{};
    float y{};

    auto operator==(Point2f const&) const -> bool = default;
};
}
