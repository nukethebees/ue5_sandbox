#pragma once
#include <ioj/constrained_numbers.h>
#include <ioj/levels/expression.h>
#include <ioj/levels/team.h>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>
namespace ioj::levels {
struct LatchDefinition {
    auto operator==(LatchDefinition const&) const -> bool = default;
    ExprIndex when{};
    ExprIndex value{};
};
struct SpawnGroupDefinition {
    auto operator==(SpawnGroupDefinition const&) const -> bool = default;
    std::string name;
    std::vector<EntityId> entities;
};
struct WinEffect {
    auto operator==(WinEffect const&) const -> bool = default;
    Team team{};
};
struct StopEffect {
    auto operator==(StopEffect const&) const -> bool = default;
};
struct SpawnEffect {
    auto operator==(SpawnEffect const&) const -> bool = default;
    std::uint32_t group{};
};
struct StartTimerEffect {
    auto operator==(StartTimerEffect const&) const -> bool = default;
    std::uint32_t timer{};
    NonNegativeFinite<double> seconds{};
};
using Effect = std::variant<WinEffect, StopEffect, SpawnEffect, StartTimerEffect>;
struct EventRule {
    auto operator==(EventRule const&) const -> bool = default;
    ExprIndex when{};
    std::vector<Effect> effects;
};
struct LogicDefinition {
    auto operator==(LogicDefinition const&) const -> bool = default;
    std::vector<Expression> expressions{};
    std::vector<LatchDefinition> latches{};
    std::uint32_t timer_count{};
    std::vector<SpawnGroupDefinition> spawn_groups{};
    std::vector<Effect> initial_effects{};
    std::vector<EventRule> rules{};
};
}
