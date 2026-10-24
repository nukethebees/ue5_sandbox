#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

namespace ioj {

// Require names to correspond to consecutive enum values starting at zero.
template <typename Enum>
    requires std::is_enum_v<Enum>
[[nodiscard]] constexpr auto try_parse_enum(std::string_view name,
                                            std::span<std::string_view const> names) noexcept
    -> std::optional<Enum> {
    auto const count{names.size()};
    for (std::size_t index{}; index < count; ++index) {
        if (name == names[index]) {
            return static_cast<Enum>(index);
        }
    }
    return std::nullopt;
}

}
