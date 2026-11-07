#pragma once

#include <ioj/constrained_value.h>
#include <ioj/vector_value_math.h>

namespace ioj {
namespace detail {
inline constexpr auto finite_vector_policy{ConstrainedValuePolicy{
    [](auto const value) constexpr noexcept -> bool { return is_finite(value); },
    "Vector components must be finite"}};

inline constexpr auto positive_finite_vector_policy{
    ConstrainedValuePolicy{[](auto const value) constexpr noexcept -> bool {
                               if (!is_finite(value)) {
                                   return false;
                               }
                               for (auto const component : value.elements) {
                                   if (component <= 0) {
                                       return false;
                                   }
                               }
                               return true;
                           },
                           "Vector components must be finite and positive"}};
}

template <std::floating_point T, std::size_t N>
using FiniteVector = ConstrainedValue<Vector<T, N>, detail::finite_vector_policy>;

template <std::floating_point T, std::size_t N>
using PositiveFiniteVector = ConstrainedValue<Vector<T, N>, detail::positive_finite_vector_policy>;
}
