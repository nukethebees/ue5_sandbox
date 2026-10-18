#pragma once
#include <cstddef>
namespace ioj::layout {
struct RowRange {
    auto operator==(RowRange const&) const -> bool = default;

    std::size_t begin{};
    std::size_t count{};
};
}
