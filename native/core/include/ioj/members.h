#pragma once

#include <type_traits>

namespace ioj {
template <typename Value>
[[nodiscard]] constexpr auto members_equal(Value const& value, auto const& instance) -> bool {
    using T = std::remove_cvref_t<Value>;
    auto const& [... members]{instance};
    static_assert((std::is_same_v<T, std::remove_cvref_t<decltype(members)>> || ...),
                  "No member matches the value type");

    template for (auto const& member : {members...}) {
        if constexpr (std::is_same_v<T, std::remove_cvref_t<decltype(member)>>) {
            if (!(member == value)) {
                return false;
            }
        }
    }
    return true;
}

template <typename Value>
constexpr void assign_members(Value const& value, auto& instance) {
    using T = std::remove_cvref_t<Value>;
    auto& [... members]{instance};
    static_assert((std::is_same_v<T, std::remove_cvref_t<decltype(members)>> || ...),
                  "No member matches the value type");

    template for (auto& member : {members...}) {
        if constexpr (std::is_same_v<T, std::remove_cvref_t<decltype(member)>>) {
            member = value;
        }
    }
}
}
