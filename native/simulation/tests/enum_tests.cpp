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
    EXPECT_EQ(to_serialized_string(Team::COUNT), "count");
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
