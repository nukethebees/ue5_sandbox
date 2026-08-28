#pragma once

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct TypeDependency {
    std::string spelling;
    std::optional<std::string> header;
    std::vector<TypeDependency> dependencies;

    auto operator==(TypeDependency const&) const -> bool = default;
};

} // namespace codegen
