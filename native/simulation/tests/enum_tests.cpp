#include <ioj/levels/mission_types.h>
#include <ioj/levels/team.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/sim/player/fire_rate.h>
#include <ioj/sim/player/space_ship_common.h>
#include <ioj/sim/telemetry/run_end_reason.h>

#include <gtest/gtest.h>

#include <utility>

namespace ioj::sim::tests {
TEST(NativeEnums, EntityTypeNamesAndSerializedNamesRemainStable) {
    for (auto const value : EnumTraits<EntityType>::values) {
        EXPECT_FALSE(to_string(value).empty());
    }
    EXPECT_EQ(to_serialized_string(EntityType::PlayerShip), "player_ship");
    EXPECT_EQ(to_serialized_string(EntityType::Turret), "turret");
    EXPECT_EQ(to_serialized_string(EntityType::CapitalShip), "capital_ship");
    EXPECT_EQ(to_serialized_string(EntityType::Fighter), "capital_ship_fighter");
    EXPECT_EQ(to_serialized_string(EntityType::TubeSpinner), "tube_spinner");
    EXPECT_EQ(to_serialized_string(static_cast<EntityType>(enum_count<EntityType>())),
              "<invalid EntityType>");
}

TEST(NativeEnums, TeamSerializedNamesRoundTrip) {
    for (auto const value : EnumTraits<Team>::values) {
        EXPECT_FALSE(to_string(value).empty());
        EXPECT_EQ(try_parse_serialized_team(to_serialized_string(value)), value);
    }
    EXPECT_EQ(to_serialized_string(Team::White), "white");
    EXPECT_EQ(to_serialized_string(Team::Red), "red");
    EXPECT_EQ(to_serialized_string(Team::Green), "green");
    EXPECT_EQ(to_serialized_string(Team::Blue), "blue");
    EXPECT_EQ(to_serialized_string(Team::Orange), "orange");
    EXPECT_EQ(to_serialized_string(Team::Yellow), "yellow");
    EXPECT_EQ(to_serialized_string(static_cast<Team>(enum_count<Team>())), "<invalid Team>");
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
    for (auto const value : {LevelTelemetryRunEndReason::MissionSucceeded,
                             LevelTelemetryRunEndReason::MissionFailed,
                             LevelTelemetryRunEndReason::BattleResolved,
                             LevelTelemetryRunEndReason::DurationReached,
                             LevelTelemetryRunEndReason::OrchestratorReset,
                             LevelTelemetryRunEndReason::WorldEnd}) {
        EXPECT_EQ(try_parse_serialized_level_telemetry_run_end_reason(to_serialized_string(value)),
                  value);
    }
    EXPECT_EQ(to_serialized_string(LevelTelemetryRunEndReason::MissionSucceeded),
              "mission_succeeded");
    EXPECT_EQ(to_serialized_string(LevelTelemetryRunEndReason::MissionFailed), "mission_failed");
    EXPECT_EQ(to_serialized_string(LevelTelemetryRunEndReason::BattleResolved), "battle_resolved");
    EXPECT_EQ(to_serialized_string(LevelTelemetryRunEndReason::DurationReached),
              "duration_reached");
    EXPECT_EQ(to_serialized_string(LevelTelemetryRunEndReason::OrchestratorReset),
              "orchestrator_reset");
    EXPECT_EQ(to_serialized_string(LevelTelemetryRunEndReason::WorldEnd), "world_end");
}

TEST(NativeEnums, PersistedEnumNumericValuesRemainStable) {
    EXPECT_EQ(std::to_underlying(EntityType::PlayerShip), 0U);
    EXPECT_EQ(std::to_underlying(EntityType::Turret), 1U);
    EXPECT_EQ(std::to_underlying(EntityType::CapitalShip), 2U);
    EXPECT_EQ(std::to_underlying(EntityType::Fighter), 3U);
    EXPECT_EQ(std::to_underlying(EntityType::TubeSpinner), 4U);
    EXPECT_EQ(enum_count<EntityType>(), 5U);

    EXPECT_EQ(std::to_underlying(Team::White), 0U);
    EXPECT_EQ(std::to_underlying(Team::Red), 1U);
    EXPECT_EQ(std::to_underlying(Team::Green), 2U);
    EXPECT_EQ(std::to_underlying(Team::Blue), 3U);
    EXPECT_EQ(std::to_underlying(Team::Orange), 4U);
    EXPECT_EQ(std::to_underlying(Team::Yellow), 5U);
    EXPECT_EQ(enum_count<Team>(), 6U);

    EXPECT_EQ(std::to_underlying(LevelTelemetryRunEndReason::MissionSucceeded), 0U);
    EXPECT_EQ(std::to_underlying(LevelTelemetryRunEndReason::MissionFailed), 1U);
    EXPECT_EQ(std::to_underlying(LevelTelemetryRunEndReason::BattleResolved), 2U);
    EXPECT_EQ(std::to_underlying(LevelTelemetryRunEndReason::DurationReached), 3U);
    EXPECT_EQ(std::to_underlying(LevelTelemetryRunEndReason::OrchestratorReset), 4U);
    EXPECT_EQ(std::to_underlying(LevelTelemetryRunEndReason::WorldEnd), 5U);
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
