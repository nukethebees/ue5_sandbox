#pragma once

#include <ioj/levels/mission_expr.h>

#include <optional>
#include <vector>

namespace ioj::levels::authoring {
// Keep references symbolic until the complete objective has been decoded.
struct ExprInput {
    MissionExpr expression{};
    std::vector<ExprInput> children{};
    std::optional<ObjectiveName> reference{};
};

struct NamedExprInput {
    ObjectiveName name;
    ExprInput value;
};

struct UnlockInput {
    LevelId level;
};
}
