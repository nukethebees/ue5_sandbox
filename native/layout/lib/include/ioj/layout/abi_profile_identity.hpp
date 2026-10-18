#pragma once

#include <optional>
#include <string>

namespace ioj::layout {

struct AbiProfileIdentity {
    auto operator==(AbiProfileIdentity const&) const -> bool = default;

    std::optional<std::string> platform;
    std::optional<std::string> architecture;
    std::optional<std::string> abi;
    std::optional<std::string> compiler;
    std::optional<std::string> build_configuration;
};

} // namespace ioj::layout
