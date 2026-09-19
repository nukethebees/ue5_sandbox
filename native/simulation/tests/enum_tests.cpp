#include <ioj/sim/entity_type.h>
#include <ioj/sim/missions/mission_fail_reason.h>
#include <ioj/sim/missions/mission_mode.h>
#include <ioj/sim/missions/mission_state.h>
#include <ioj/sim/player/control_mode.h>
#include <ioj/sim/player/fire_rate.h>
#include <ioj/sim/player/flight_mode.h>
#include <ioj/sim/player/laser_firing_state.h>
#include <ioj/sim/player/ship_laser_mode.h>
#include <ioj/sim/player/space_ship_common.h>
#include <ioj/sim/team.h>
#include <ioj/sim/telemetry/level_telemetry_run_end_reason.h>

#include <gtest/gtest.h>

#include <utility>

namespace ioj::sim::tests {
TEST(NativeEnums, EntityTypeNamesAndSerializedNamesRoundTrip) {
    EXPECT_EQ(ml::EnumTraits<EntityType>::count, 5U);
    for (auto const value : ml::EnumTraits<EntityType>::values) {
        EXPECT_FALSE(to_string(value).empty());
        EXPECT_EQ(try_parse_entity_type(to_string(value)), value);
        EXPECT_EQ(try_parse_serialized_entity_type(to_serialized_string(value)), value);
    }
    EXPECT_FALSE(try_parse_entity_type("unknown").has_value());
    EXPECT_FALSE(try_parse_serialized_entity_type("unknown").has_value());
    EXPECT_EQ(std::to_underlying(EntityType::PlayerShip), 0U);
    EXPECT_EQ(std::to_underlying(EntityType::Fighter), 3U);
    EXPECT_EQ(to_serialized_string(EntityType::PlayerShip), "player_ship");
    EXPECT_EQ(to_serialized_string(EntityType::Turret), "turret");
    EXPECT_EQ(to_serialized_string(EntityType::CapitalShip), "capital_ship");
    EXPECT_EQ(to_serialized_string(EntityType::Fighter), "capital_ship_fighter");
    EXPECT_EQ(to_serialized_string(EntityType::TubeSpinner), "tube_spinner");
    EXPECT_EQ(to_serialized_string(EntityType::COUNT), "<invalid EntityType>");
    EXPECT_EQ(try_parse_entity_type("COUNT"), EntityType::COUNT);
    EXPECT_FALSE(try_parse_serialized_entity_type("count").has_value());
}

TEST(NativeEnums, TeamNamesAndSerializedNamesRoundTrip) {
    EXPECT_EQ(ml::EnumTraits<Team>::count, 6U);
    for (auto const value : ml::EnumTraits<Team>::values) {
        EXPECT_FALSE(to_string(value).empty());
        EXPECT_EQ(try_parse_team(to_string(value)), value);
        EXPECT_EQ(try_parse_serialized_team(to_serialized_string(value)), value);
    }
    EXPECT_EQ(std::to_underlying(Team::White), 0U);
    EXPECT_EQ(std::to_underlying(Team::Yellow), 5U);
    EXPECT_EQ(to_serialized_string(Team::White), "white");
    EXPECT_EQ(to_serialized_string(Team::Red), "red");
    EXPECT_EQ(to_serialized_string(Team::Green), "green");
    EXPECT_EQ(to_serialized_string(Team::Blue), "blue");
    EXPECT_EQ(to_serialized_string(Team::Orange), "orange");
    EXPECT_EQ(to_serialized_string(Team::Yellow), "yellow");
    EXPECT_EQ(to_serialized_string(Team::COUNT), "count");
    EXPECT_EQ(try_parse_team("COUNT"), Team::COUNT);
    EXPECT_EQ(try_parse_serialized_team("count"), Team::COUNT);
}

TEST(NativeEnums, MissionAndTelemetrySerializedNamesRoundTrip) {
    for (auto const value : ml::EnumTraits<MissionMode>::values) {
        EXPECT_EQ(try_parse_serialized_mission_mode(to_serialized_string(value)), value);
    }
    for (auto const value : ml::EnumTraits<MissionState>::values) {
        EXPECT_EQ(try_parse_serialized_mission_state(to_serialized_string(value)), value);
    }
    for (auto const value : ml::EnumTraits<MissionFailReason>::values) {
        EXPECT_EQ(try_parse_serialized_mission_fail_reason(to_serialized_string(value)), value);
    }
    for (auto const value : ml::EnumTraits<LevelTelemetryRunEndReason>::values) {
        EXPECT_EQ(try_parse_serialized_level_telemetry_run_end_reason(to_serialized_string(value)),
                  value);
    }
    EXPECT_EQ(std::to_underlying(LevelTelemetryRunEndReason::WorldEnd), 5U);
    EXPECT_EQ(to_serialized_string(MissionMode::None), "none");
    EXPECT_EQ(to_serialized_string(MissionMode::SurviveTime), "survive_time");
    EXPECT_EQ(to_serialized_string(MissionMode::KillEnemies), "kill_enemies");
    EXPECT_EQ(to_serialized_string(MissionMode::KillEnemiesWithinTime), "kill_enemies_within_time");
    EXPECT_EQ(to_serialized_string(MissionState::NotStarted), "not_started");
    EXPECT_EQ(to_serialized_string(MissionState::Running), "running");
    EXPECT_EQ(to_serialized_string(MissionState::Succeeded), "succeeded");
    EXPECT_EQ(to_serialized_string(MissionState::Failed), "failed");
    EXPECT_EQ(to_serialized_string(MissionState::Disabled), "disabled");
    EXPECT_EQ(to_serialized_string(MissionFailReason::None), "none");
    EXPECT_EQ(to_serialized_string(MissionFailReason::PlayerKilled), "player_killed");
    EXPECT_EQ(to_serialized_string(MissionFailReason::TimeElapsed), "time_elapsed");
    EXPECT_EQ(to_serialized_string(MissionFailReason::DefenceObjectiveFailed),
              "defence_objective_failed");
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
    EXPECT_EQ(std::to_underlying(EntityType::COUNT), 5U);

    EXPECT_EQ(std::to_underlying(Team::White), 0U);
    EXPECT_EQ(std::to_underlying(Team::Red), 1U);
    EXPECT_EQ(std::to_underlying(Team::Green), 2U);
    EXPECT_EQ(std::to_underlying(Team::Blue), 3U);
    EXPECT_EQ(std::to_underlying(Team::Orange), 4U);
    EXPECT_EQ(std::to_underlying(Team::Yellow), 5U);
    EXPECT_EQ(std::to_underlying(Team::COUNT), 6U);

    EXPECT_EQ(std::to_underlying(MissionMode::None), 0U);
    EXPECT_EQ(std::to_underlying(MissionMode::SurviveTime), 1U);
    EXPECT_EQ(std::to_underlying(MissionMode::KillEnemies), 2U);
    EXPECT_EQ(std::to_underlying(MissionMode::KillEnemiesWithinTime), 3U);

    EXPECT_EQ(std::to_underlying(MissionState::NotStarted), 0U);
    EXPECT_EQ(std::to_underlying(MissionState::Running), 1U);
    EXPECT_EQ(std::to_underlying(MissionState::Succeeded), 2U);
    EXPECT_EQ(std::to_underlying(MissionState::Failed), 3U);
    EXPECT_EQ(std::to_underlying(MissionState::Disabled), 4U);

    EXPECT_EQ(std::to_underlying(MissionFailReason::None), 0U);
    EXPECT_EQ(std::to_underlying(MissionFailReason::PlayerKilled), 1U);
    EXPECT_EQ(std::to_underlying(MissionFailReason::TimeElapsed), 2U);
    EXPECT_EQ(std::to_underlying(MissionFailReason::DefenceObjectiveFailed), 3U);

    EXPECT_EQ(std::to_underlying(LevelTelemetryRunEndReason::MissionSucceeded), 0U);
    EXPECT_EQ(std::to_underlying(LevelTelemetryRunEndReason::MissionFailed), 1U);
    EXPECT_EQ(std::to_underlying(LevelTelemetryRunEndReason::BattleResolved), 2U);
    EXPECT_EQ(std::to_underlying(LevelTelemetryRunEndReason::DurationReached), 3U);
    EXPECT_EQ(std::to_underlying(LevelTelemetryRunEndReason::OrchestratorReset), 4U);
    EXPECT_EQ(std::to_underlying(LevelTelemetryRunEndReason::WorldEnd), 5U);
}

TEST(NativeEnums, PlayerEnumsExposeExhaustiveNativeValues) {
    EXPECT_EQ(ml::EnumTraits<ShipLaserMode>::count, 3U);
    EXPECT_EQ(ml::EnumTraits<LaserFiringState>::count, 5U);
    EXPECT_EQ(ml::EnumTraits<ShipFireRate>::count, 3U);
    EXPECT_EQ(ml::EnumTraits<SpaceShipFlightMode>::count, 2U);
    EXPECT_EQ(ml::EnumTraits<SpaceShipControlMode>::count, 2U);
    EXPECT_EQ(ml::EnumTraits<player::BoostBrakeState>::count, 3U);

    for (auto const value : ml::EnumTraits<ShipLaserMode>::values) {
        EXPECT_FALSE(to_string(value).empty());
    }
    for (auto const value : ml::EnumTraits<LaserFiringState>::values) {
        EXPECT_FALSE(to_string(value).empty());
    }
    for (auto const value : ml::EnumTraits<ShipFireRate>::values) {
        EXPECT_FALSE(to_string(value).empty());
    }
    for (auto const value : ml::EnumTraits<SpaceShipFlightMode>::values) {
        EXPECT_FALSE(to_string(value).empty());
    }
    for (auto const value : ml::EnumTraits<SpaceShipControlMode>::values) {
        EXPECT_FALSE(to_string(value).empty());
    }
    for (auto const value : ml::EnumTraits<player::BoostBrakeState>::values) {
        EXPECT_FALSE(to_string(value).empty());
    }
    EXPECT_EQ(to_string(player::BoostBrakeState::Boost), "Boost");
}
} // namespace ioj::sim::tests
