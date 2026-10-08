#pragma once

#include <optional>
#include <string_view>
#include <unordered_map>

namespace ioj {
template <typename T>
[[nodiscard]] auto lookup_enum(std::unordered_map<std::string_view, T> const& values,
                               std::string_view const name) -> std::optional<T> {
    auto const found{values.find(name)};
    if (found == values.end()) {
        return std::nullopt;
    }
    return found->second;
}
}
