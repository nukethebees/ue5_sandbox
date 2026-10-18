#pragma once

#include <cstddef>
#include <string>

namespace ioj::layout {

struct AbiProfileParseError {
    auto operator==(AbiProfileParseError const&) const -> bool = default;

    std::size_t line{};
    std::string message;
};

} // namespace ioj::layout
