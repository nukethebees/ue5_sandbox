#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/vector.h>

#include <algorithm>

namespace ioj {
namespace detail {
template <typename ScalarConstraint>
struct EveryComponent {
    [[nodiscard]] constexpr auto operator()(auto const& value) const noexcept -> bool {
        return std::ranges::all_of(value.elements, ScalarConstraint::valid);
    }
};
}

template <std::floating_point T, std::size_t N>
using FiniteVector = ConstrainedValue<Vector<T, N>,
                                      ConstrainedValuePolicy{detail::EveryComponent<Finite<T>>{},
                                                             "Vector components must be finite"}>;

template <std::floating_point T, std::size_t N>
using PositiveFiniteVector =
    ConstrainedValue<Vector<T, N>,
                     ConstrainedValuePolicy{detail::EveryComponent<PositiveFinite<T>>{},
                                            "Vector components must be finite and positive"}>;
}
