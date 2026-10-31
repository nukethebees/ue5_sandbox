#pragma once

#include <cstddef>
#include <span>

namespace ioj {
struct Subspan {
    template <typename Range>
    [[nodiscard]] constexpr auto operator()(Range& values) const {
        return std::span{values}.subspan(offset, count);
    }

    template <typename View, typename... Ranges>
    [[nodiscard]] constexpr auto make_view(Ranges&... values) const -> View {
        return View{(*this)(values)...};
    }

    std::size_t offset{};
    std::size_t count{};
};
}
