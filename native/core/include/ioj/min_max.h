#pragma once

namespace ioj {
template <typename T>
struct MinMax {
    T min{};
    T max{1};

    auto operator==(MinMax const&) const -> bool = default;
};
}
