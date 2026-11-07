#pragma once

#include <ioj/levels/mission_expr.h>

#include <optional>
#include <vector>

namespace ioj::levels::authoring {
// Keep references symbolic until the complete objective has been decoded.
struct ExprInput {
    auto operator==(ExprInput const&) const -> bool = default;

    MissionExpr expression{};
    std::vector<ExprInput> children{};
    std::optional<ObjectiveName> reference{};
};

}
