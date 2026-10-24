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
    auto operator==(ObjectiveDependency const&) const -> bool = default;
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
    auto operator==(MissionExpression const& other) const -> bool {
        return type == other.type && children == other.children && entity == other.entity &&
               entities == other.entities && position.x() == other.position.x() &&
               position.y() == other.position.y() && position.z() == other.position.z() &&
               radius == other.radius && seconds == other.seconds && kills == other.kills &&
               objective == other.objective;
    }
};

struct ObjectiveDefinition {
    std::string id{};
    std::string title{};
    Team team{Team::White};
    std::vector<MissionExpression> expressions{};
    // Indexed by ObjectiveOutcome, in pass/fail/expire precedence order.
    std::array<ExpressionIndex, 3> roots{absent_expression, absent_expression, absent_expression};
    std::vector<ObjectiveDependency> dependencies{};
    auto operator==(ObjectiveDefinition const&) const -> bool = default;
};

struct TeamResultDefinition {
    Team team{Team::White};
    ObjectiveDependency when{};
    TeamOutcome result{TeamOutcome::Victory};
    auto operator==(TeamResultDefinition const&) const -> bool = default;
};

struct MissionDefinition {
    std::vector<ObjectiveDefinition> objectives{};
    std::vector<TeamResultDefinition> team_results{};
    auto operator==(MissionDefinition const&) const -> bool = default;
};
}
