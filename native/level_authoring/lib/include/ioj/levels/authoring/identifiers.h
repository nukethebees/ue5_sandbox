#pragma once

#include <string>
#include <string_view>

namespace ioj::levels::authoring {
[[nodiscard]] auto is_canonical_symbol(std::string_view value) -> bool;
// The fallback must already be canonical. Non-ASCII bytes are separators, not letters.
[[nodiscard]] auto canonical_symbol(std::string_view text, std::string_view fallback)
    -> std::string;
}
