#include <ioj/levels/mission_types.h>
#include <ioj/levels/team.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/sim/player/fire_rate.h>
#include <ioj/sim/player/space_ship_common.h>
#include <ioj/sim/telemetry/run_end_reason.h>

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

TEST(NativeEnums, MissionAndTelemetrySerializedNamesRoundTrip) {
    for (auto const value :
         {ObjectiveOutcome::Passed, ObjectiveOutcome::Failed, ObjectiveOutcome::Expired}) {
        EXPECT_EQ(try_parse_serialized_objective_outcome(to_serialized_string(value)), value);
    }
    for (auto const value : {TeamOutcome::Victory, TeamOutcome::Defeat}) {
        EXPECT_EQ(try_parse_serialized_team_outcome(to_serialized_string(value)), value);
    }
    using enum LevelTelemetryRunEndReason;
    std::array const reasons{MissionSucceeded,
                             MissionFailed,
                             BattleResolved,
                             DurationReached,
                             OrchestratorReset,
                             WorldEnd};
    for (auto const value : reasons) {
        EXPECT_EQ(try_parse_serialized_level_telemetry_run_end_reason(to_serialized_string(value)),
                  value);
    }
    expect_enum_contracts(reasons,
                          std::array{"mission_succeeded",
                                     "mission_failed",
                                     "battle_resolved",
                                     "duration_reached",
                                     "orchestrator_reset",
                                     "world_end"});
}

TEST(NativeEnums, PlayerEnumsPreserveHudNamesAndFireRateCount) {
    EXPECT_EQ(enum_count<ShipFireRate>(), 3U);
    EXPECT_EQ(to_string(ShipFireRate::Single), "Single");
    EXPECT_EQ(to_string(ShipFireRate::Burst3), "Burst3");
    EXPECT_EQ(to_string(ShipFireRate::FullAuto), "FullAuto");
    EXPECT_EQ(to_string(player::BoostBrakeState::None), "None");
    EXPECT_EQ(to_string(player::BoostBrakeState::Boost), "Boost");
    EXPECT_EQ(to_string(player::BoostBrakeState::Brake), "Brake");
    EXPECT_EQ(to_string(player::BoostBrakeState::EmergencyBrake), "EmergencyBrake");
}
} // namespace ioj::sim::tests
