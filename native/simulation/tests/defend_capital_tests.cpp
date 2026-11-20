#include "../../level_authoring/tests/module_fixture.h"
#include "support/simulation_test_support.h"
#include <ioj/column_math.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/sim/levels/level_simulation_builder.h>
#include <ioj/sim/testing/level_sim_test_access.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <vector>

namespace ioj::sim::tests::defend_capital {
auto script_root() -> std::filesystem::path {
    return std::filesystem::path{IOJ_PROJECT_SOURCE_DIR} / "LevelScripts";
}

auto capital_id(LevelSim const& simulation, levels::LevelDefinition const& level, char const* name)
    -> EntityId {
    auto const& capital{level.entity(levels::EntityId{name})};
    auto const index{&capital - level.entities().data()};
    return LevelSimTestAccess::level_entity_ids(simulation)[index];
}

auto first_wave_history(EntityLedger const& ledger, std::uint32_t count, Team team) -> std::string {
    auto const history{ledger.history()};
    std::string report;
    for (std::uint32_t i{}; i < count; ++i) {
        auto const row{ledger.history_index(EntityId{i, EntityType::Fighter})};
        if (history.teams()[row] != team) {
            continue;
        }
        auto const state{history.life_state()[row]};
        if (state == LifeState::Alive) {
            report += std::format(" #{} alive;", i);
        } else {
            report += std::format(" #{} {} death at tick {};",
                                  i,
                                  state == LifeState::Combat ? "combat" : "unattributed",
                                  history.death_ticks()[row].value());
        }
    }
    return report;
}

TEST(DefendCapital, BothTeamsLeaveTheirLaunchSlotsToAttackTheOpposingObjective) {
    auto const path{std::filesystem::path{IOJ_PROJECT_SOURCE_DIR} /
                    "native/simulation/tests/fixtures/defend_capital.scm"};
    auto const level{levels::authoring::DefinitionReader{script_root()}.read_level_file(path)};
    ASSERT_TRUE(level) << levels::format_diagnostics(level.error());

    auto built{make_level_simulation_init_data(*level, {}, {})};
    ASSERT_TRUE(built) << built.error().front();
    LevelSim simulation{std::move(*built)};
    auto const blue_capital{capital_id(simulation, *level, "capitalship-blue")};
    auto const red_objective{capital_id(simulation, *level, "capitalship-red2")};
    auto const clearance{
        2.f * simulation.spatial_query_manager().entity_type_radius(EntityType::Fighter)};
    auto const dt{simulation.clock().tick_period()};
    auto const tick_limit{static_cast<int>(std::round(80.0 / dt))};

    struct Launch {
        EntityId id;
        Vector3f position;
    };
    std::vector<Launch> first_wave;
    std::uint32_t first_wave_issued{};
    bool blue_departed{};
    bool red_departed{};
    simulation.start();
    for (int tick{}; tick < tick_limit && !simulation.result(); ++tick) {
        simulation.advance(dt);
        auto const fighters{simulation.fighters().entities()};
        auto const count{fighters.num()};
        auto const ids{fighters.entity_ids()};
        auto const positions{fighters.view_locations().resolve()};

        auto const issued{simulation.entity_ledger().issued_counts()[EntityType::Fighter]};
        if (first_wave_issued == 0) {
            first_wave_issued = issued;
            for (std::uint32_t row{}; row < count; ++row) {
                first_wave.push_back({ids[row], vector_at(positions, row)});
            }
            continue;
        }
        if (issued > first_wave_issued) {
            break;
        }

        auto const teams{fighters.teams()};
        auto const targets{fighters.target_ids()};
        for (std::uint32_t row{}; row < count; ++row) {
            auto const launch{std::ranges::find(first_wave, ids[row], &Launch::id)};
            ASSERT_NE(launch, first_wave.end());
            if (size(vector_at(positions, row) - launch->position) > clearance) {
                blue_departed |= teams[row] == Team::Blue && targets[row] == red_objective;
                red_departed |= teams[row] == Team::Red && targets[row] == blue_capital;
            }
        }
        if (blue_departed && red_departed) {
            break;
        }
    }

    ASSERT_GT(first_wave_issued, 0u) << "Capitals launch fighters";
    EXPECT_TRUE(blue_departed)
        << "A Blue fighter clears its launch slot targeting capitalship-red2."
        << first_wave_history(simulation.entity_ledger(), first_wave_issued, Team::Blue);
    EXPECT_TRUE(red_departed) << "A Red fighter clears its launch slot targeting capitalship-blue."
                              << first_wave_history(
                                     simulation.entity_ledger(), first_wave_issued, Team::Red);
    for (auto tick{static_cast<int>(simulation.clock().get_completed_ticks().value())};
         tick < tick_limit && !simulation.result();
         ++tick) {
        simulation.advance(dt);
    }
    EXPECT_TRUE(simulation.result()) << "The mission finishes within 80 simulated seconds";
}

struct OutcomeCase {
    char const* name;
    double seconds_before_decision;
    std::vector<Team> destroyed_capitals;
    Team winner;
};

class DefendCapitalOutcome : public ::testing::TestWithParam<OutcomeCase> {};

TEST_P(DefendCapitalOutcome, AwardsVictoryOnTheDecidingTick) {
    auto const& scenario{GetParam()};
    // Keep the live mission rules but isolate their inputs from incidental combat.
    auto const level{ioj::levels::authoring::tests::read_test_level(
        levels::authoring::DefinitionReader{script_root()}, R"(
      (copy-with (module-ref (import-module "defend_capital.scm") 'mission)
        :settings (settings :tick (tick :tick-rate 1))
        :entities (list
          (entity :id 'playership :archetype 'player-fighter :team 'blue :position '(0 10000 0))
          (entity :id 'capitalship-blue :archetype 'capital-ship :team 'blue
                  :position '(-20000 0 0) :initial-spawn-delay 90)
          (entity :id 'capitalship-red2 :archetype 'capital-ship :team 'red
                  :position '(20000 0 0) :initial-spawn-delay 90))))")};
    ASSERT_TRUE(level) << levels::format_diagnostics(level.error());
    auto built{make_level_simulation_init_data(*level, {}, {})};
    ASSERT_TRUE(built) << built.error().front();
    WorldlessSimulationTest harness{std::move(*built)};
    auto& simulation{harness.simulation()};
    simulation.start();
    simulation.advance(scenario.seconds_before_decision);
    ASSERT_FALSE(simulation.result());

    std::vector<EntityId> victims;
    for (auto const team : scenario.destroyed_capitals) {
        victims.push_back(capital_id(
            simulation, *level, team == Team::Blue ? "capitalship-blue" : "capitalship-red2"));
    }
    harness.queue_kills(victims, {});
    simulation.advance(simulation.clock().tick_period());
    ASSERT_TRUE(simulation.result());
    EXPECT_EQ(simulation.result()->winner, scenario.winner);
}

INSTANTIATE_TEST_SUITE_P(
    MissionRules,
    DefendCapitalOutcome,
    ::testing::Values(OutcomeCase{"RedObjectiveDestroyedEarly", 1, {Team::Red}, Team::Blue},
                      OutcomeCase{"BlueCapitalDestroyedEarly", 1, {Team::Blue}, Team::Red},
                      OutcomeCase{"RedObjectiveSurvivesDeadline", 59, {}, Team::Red},
                      OutcomeCase{"RedObjectiveDestroyedOnDeadline", 59, {Team::Red}, Team::Blue},
                      OutcomeCase{
                          "SimultaneousCapitalLosses", 1, {Team::Blue, Team::Red}, Team::Blue}),
    [](auto const& info) { return info.param.name; });
}
