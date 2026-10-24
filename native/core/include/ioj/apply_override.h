#pragma once

#include <optional>
#include <type_traits>

namespace ioj {

template <typename T, typename Override>
constexpr void apply_override(T& target, std::optional<Override> const& value) {
    if (value) {
        if constexpr (std::is_same_v<T, Override>) {
            target = *value;
        } else {
            apply_overrides(target, *value);
        }
    }
}

}
