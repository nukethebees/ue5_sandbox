#pragma once

#include <ioj/levels/identifiers.h>
#include <ioj/levels/mission_types.h>
#include <ioj/levels/team.h>
#include <ioj/math_types.h>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace ioj::levels {
using ExpressionIndex = std::uint32_t;
inline constexpr ExpressionIndex absent_expression{std::numeric_limits<ExpressionIndex>::max()};

struct ObjectiveDependency {
    std::string objective{};
    ObjectiveOutcome outcome{ObjectiveOutcome::Passed};
};

// Authored expressions use local indices. Only outcome conditions cross objective boundaries.
struct MissionExpression {
    ExpressionType type{ExpressionType::All};
    std::vector<ExpressionIndex> children{};
    EntityId entity{};
    std::vector<EntityId> entities{};
    Vector3d position{};
    double radius{};
    double seconds{};
    std::uint32_t kills{};
    ObjectiveDependency objective{};
};

struct ObjectiveDefinition {
    std::string id{};
    std::string title{};
    Team team{Team::White};
    std::vector<MissionExpression> expressions{};
    // Indexed by ObjectiveOutcome, in pass/fail/expire precedence order.
    std::array<ExpressionIndex, 3> roots{absent_expression, absent_expression, absent_expression};
    std::vector<ObjectiveDependency> dependencies{};
};

struct TeamResultDefinition {
    Team team{Team::White};
    ObjectiveDependency when{};
    TeamOutcome result{TeamOutcome::Victory};
};

struct MissionDefinition {
    std::vector<ObjectiveDefinition> objectives{};
    std::vector<TeamResultDefinition> team_results{};
};
}
