#include <ioj/expr_kind.h>
#include <ioj/levels/level_outcome.h>
#include <ioj/levels/team.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/sim/expressions/expr_handle.h>
#include <ioj/sim/expressions/leaf_data_handle.h>
#include <ioj/sim/levels/run_end_reason.h>
#include <ioj/sim/player/space_ship_common.h>
#include <ioj/sim/player/trigger_mode.h>

#include <gtest/gtest.h>

#include <array>
#include <utility>

namespace ioj::sim::tests {
template <typename Enum, std::size_t Count>
void expect_enum_contracts(std::array<Enum, Count> const& values,
                           std::array<char const*, Count> const& names) {
    for (std::size_t index{}; index < Count; ++index) {
        SCOPED_TRACE(names[index]);
        EXPECT_EQ(to_serialized_string(values[index]), names[index]);
        EXPECT_EQ(std::to_underlying(values[index]), index);
    }
}

TEST(NativeEnums, EntityTypeNamesAndSerializedNamesRemainStable) {
    for (auto const value : EnumTraits<EntityType>::values) {
        EXPECT_FALSE(to_string(value).empty());
    }
    using enum EntityType;
    expect_enum_contracts(
        std::array{PlayerShip, Turret, CapitalShip, Fighter, TubeSpinner},
        std::array{
            "player_ship", "turret", "capital_ship", "capital_ship_fighter", "tube_spinner"});
    EXPECT_EQ(enum_count<EntityType>(), 5U);
}

TEST(NativeEnums, ExpressionKindsPreserveNumericValuesAndSerializedNames) {
    using enum ExprKind;
    std::array const kinds{All,
                           Any,
                           Not,
                           Xor,
                           EntityDestroyed,
                           EntityReachedPosition,
                           TimerElapsed,
                           LatchedTrue,
                           LatchedFalse,
                           EntityKillsReached};
    expect_enum_contracts(kinds,
                          std::array{"all-of",
                                     "any-of",
                                     "not-of",
                                     "xor-of",
                                     "entity-destroyed",
                                     "entity-reached-position",
                                     "timer-elapsed",
                                     "latched-true",
                                     "latched-false",
                                     "entity-kills-reached"});
    for (auto const kind : kinds) {
        EXPECT_EQ(try_parse_serialized_expr_kind(to_serialized_string(kind)), kind);
        EXPECT_EQ(is_composite(kind), std::to_underlying(kind) < 4);
        EXPECT_EQ(is_leaf(kind), std::to_underlying(kind) >= 4);
    }
    static_assert(expr_kind_composite_range.first() == All);
    static_assert(expr_kind_composite_range.last() == Xor);
    static_assert(expr_kind_leaf_range.first() == EntityDestroyed);
    static_assert(expr_kind_leaf_range.last() == EntityKillsReached);
    EXPECT_FALSE(try_parse_serialized_expr_kind("leaf"));
}

TEST(NativeEnums, ExpressionHandlesPackEveryKindWithBoundaryIndices) {
    static_assert(sizeof(ExprHandle) == 4);
    static_assert(sizeof(LeafDataHandle) == 4);
    static_assert(ExprHandle{0x0fffffff, ExprKind::EntityKillsReached}.raw_value() == 0x9fffffff);
    static_assert(!ExprHandle{}.is_valid());
    static_assert(ExprHandle{}.raw_value() == 0xffffffff);
    EXPECT_FALSE(ExprHandle::from_raw(0xffffffff).is_valid());
    EXPECT_FALSE(ExprHandle::is_valid_index(0x10000000));
    for (auto const kind : EnumTraits<ExprKind>::values) {
        for (auto const index : {0u, 1u, 0x0fffffffu}) {
            ExprHandle const handle{index, kind};
            EXPECT_TRUE(handle.is_valid());
            EXPECT_EQ(handle.index(), index);
            EXPECT_EQ(handle.kind(), kind);
            EXPECT_EQ(handle.raw_value(), index | (std::uint32_t{std::to_underlying(kind)} << 28));
            EXPECT_EQ(ExprHandle::from_raw(handle.raw_value()), handle);
        }
    }
    for (auto const kind : expr_kind_leaf_values) {
        for (auto const index : {0u, 1u, 0x00ffffffu}) {
            LeafDataHandle const handle{index, kind};
            EXPECT_TRUE(handle.is_valid());
            EXPECT_EQ(handle.index(), index);
            EXPECT_EQ(handle.kind(), kind);
            EXPECT_EQ(handle.raw_value(), index | (std::uint32_t{std::to_underlying(kind)} << 24));
        }
    }
}

TEST(NativeEnums, TeamSerializedNamesRoundTrip) {
    for (auto const value : EnumTraits<Team>::values) {
        EXPECT_FALSE(to_string(value).empty());
        EXPECT_EQ(try_parse_serialized_team(to_serialized_string(value)), value);
    }
    using enum Team;
    expect_enum_contracts(std::array{White, Red, Green, Blue, Orange, Yellow},
                          std::array{"white", "red", "green", "blue", "orange", "yellow"});
    EXPECT_EQ(enum_count<Team>(), 6U);
    EXPECT_FALSE(try_parse_serialized_team("count").has_value());
}

TEST(NativeEnums, LevelOutcomeAndEndReasonSerializedNamesRoundTrip) {
    for (auto const value : {LevelOutcome::Victory, LevelOutcome::Defeat, LevelOutcome::Stopped}) {
        EXPECT_EQ(try_parse_serialized_level_outcome(to_serialized_string(value)), value);
    }
    using enum RunEndReason;
    std::array const reasons{LevelCompleted, DurationReached, OrchestratorReset, WorldEnd};
    for (auto const value : reasons) {
        EXPECT_EQ(try_parse_serialized_run_end_reason(to_serialized_string(value)), value);
    }
    expect_enum_contracts(
        reasons,
        std::array{"level_completed", "duration_reached", "orchestrator_reset", "world_end"});
}

TEST(NativeEnums, PlayerEnumsPreserveHudNamesAndFireRateCount) {
    EXPECT_EQ(enum_count<ShipTriggerMode>(), 3U);
    EXPECT_EQ(to_string(ShipTriggerMode::Single), "Single");
    EXPECT_EQ(to_string(ShipTriggerMode::Burst3), "Burst3");
    EXPECT_EQ(to_string(ShipTriggerMode::FullAuto), "FullAuto");
    EXPECT_EQ(to_string(player::BoostBrakeState::None), "None");
    EXPECT_EQ(to_string(player::BoostBrakeState::Boost), "Boost");
    EXPECT_EQ(to_string(player::BoostBrakeState::Brake), "Brake");
    EXPECT_EQ(to_string(player::BoostBrakeState::EmergencyBrake), "EmergencyBrake");
}
} // namespace ioj::sim::tests
