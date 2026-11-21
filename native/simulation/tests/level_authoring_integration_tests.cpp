#include "../../level_authoring/tests/module_fixture.h"
#include "support/simulation_test_support.h"
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/sim/levels/level_simulation_builder.h>
#include <ioj/sim/testing/level_sim_test_access.h>

#include <filesystem>
#include <format>

namespace ioj::sim::tests {
namespace {
auto mission_reader() -> levels::authoring::DefinitionReader {
    return levels::authoring::DefinitionReader{std::filesystem::path{IOJ_PROJECT_SOURCE_DIR} /
                                               "LevelScripts"};
}

void expect_fighter_kill_from_objective(std::string_view const win_condition) {
    auto const definition{
        ioj::levels::authoring::tests::read_test_level(mission_reader(),
                                                       std::format(R"(
      (level :id 'fighter-kill :title "Fighter kill"
        :camera (camera :look-at '(blue-capital) :offset-direction '(-1 0 0) :distance 1000)
        :settings (settings
          :entity-geometry (module-ref (import-module "Settings/entity-geometry.scm") 'entity-geometry)
          :fighters (fighters
            :max-live-auto-spawned-fighters 2
            :laser (laser :damage 100 :projectile-speed 40000 :max-distance 40000)))
        :entities (list
          (entity :id 'blue-capital :archetype 'capital-ship :team 'blue
            :position '(-20000 0 0) :initial-spawn-delay 0 :spawn-cooldown 60)
          (entity :id 'red-capital :archetype 'capital-ship :team 'red
            :position '(20000 0 0) :health 30 :initial-spawn-delay 60 :spawn-cooldown 60))
        :logic (logic
          (define target-dead (entity-destroyed 'red-capital))
          (on :when {} :do (win 'blue)))))",
                                                                   win_condition))};
    ASSERT_TRUE(definition) << levels::format_diagnostics(definition.error());

    auto built{make_level_simulation_init_data(*definition, {}, {})};
    ASSERT_TRUE(built) << built.error().front();
    LevelSim simulation{std::move(*built)};

    auto const capitals{simulation.capital_ships().get_entities()};
    ASSERT_EQ(capitals.num(), 2);
    auto const red{capitals.entity_ids()[1]};

    simulation.start();
    simulation.advance(10.0);

    auto const fighters{simulation.fighters().entities()};
    ASSERT_EQ(fighters.num(), 1);
    EXPECT_EQ(fighters.teams()[0], Team::Blue);
    auto const& ledger{simulation.entity_ledger()};
    EXPECT_EQ(ledger.issued_counts()[EntityType::Fighter], 1);
    EXPECT_EQ(ledger.history().life_state()[ledger.history_index(red)], LifeState::Combat);
    EXPECT_EQ(ledger.kills(fighters.entity_ids()[0]), 1u);
}
}

TEST(LevelAuthoringIntegration, BlueFighterDestroysRedCapitalFromWinObjective) {
    expect_fighter_kill_from_objective("target-dead");
}

TEST(LevelAuthoringIntegration, BlueFighterAttacksTheValueOfALatchedWinCondition) {
    expect_fighter_kill_from_objective(R"(
        (latched-true (latch :when (any-of (entity-destroyed 'blue-capital) target-dead)
                             :value target-dead)))");
}

TEST(LevelAuthoringIntegration, ExplicitGroupsSpawnOnceAndPreserveDeathReferences) {
    auto const definition{ioj::levels::authoring::tests::read_test_level(mission_reader(), R"(
      (level :id 'reserves :title "Reserves"
        :camera (camera :look-at '(hero) :offset-direction '(-1 0 0) :distance 1000)
        :entities (list
          (entity :id 'hero :archetype 'capital-ship :team 'blue :position '(-20000 0 0) :initial-spawn-delay 60)
          (entity :id 'reserve :archetype 'capital-ship :team 'red :position '(20000 0 0) :initial-spawn-delay 60)
          (entity :id 'turret :archetype 'static-turret :team 'red :position '(0 20000 0)))
        :logic (logic
          (define group (spawn-group 'reserve '(reserve turret)))
          (define ready (timer))
          (define dead (all-of (entity-destroyed 'reserve) (entity-destroyed 'turret)))
          (define remembered (latch :when dead :value dead))
          (initially (start-timer ready 0))
          (on :when (timer-elapsed ready) :do (list (spawn group) (spawn group)))
          (on :when dead :do (spawn group))
          (on :when (latched-true remembered) :do (win 'blue)))))")};
    ASSERT_TRUE(definition) << levels::format_diagnostics(definition.error());
    auto build{[&] { return make_level_simulation_init_data(*definition, {}, {}); }};
    auto built{build()};
    ASSERT_TRUE(built);
    LevelSim simulation{std::move(*built)};

    EXPECT_EQ(simulation.capital_ships().num_instances(), 1);
    EXPECT_EQ(simulation.turrets().num_instances(), 0);
    simulation.start();
    auto const dt{simulation.clock().tick_period()};
    simulation.advance(dt);
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1);
    simulation.advance(dt);
    ASSERT_EQ(simulation.capital_ships().num_instances(), 2);
    ASSERT_EQ(simulation.turrets().num_instances(), 1);
    DirectDamageEvents damage;
    damage.add(simulation.capital_ships().get_entities().entity_ids()[1], 100000, {});
    damage.add(simulation.turrets().get_entities().entity_ids()[0], 100000, {});
    LevelSimTestAccess::queue_direct_damage_events(simulation, damage.const_view());
    simulation.advance(dt);
    EXPECT_FALSE(simulation.result());
    simulation.advance(dt);
    ASSERT_TRUE(simulation.result());
    EXPECT_EQ(simulation.result()->winner, Team::Blue);
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1);
    EXPECT_EQ(simulation.turrets().num_instances(), 0);
    auto const& issued_counts{simulation.entity_ledger().issued_counts()};
    EXPECT_EQ(issued_counts[EntityType::CapitalShip], 2);
    EXPECT_EQ(issued_counts[EntityType::Turret], 1);

    auto fresh{build()};
    ASSERT_TRUE(fresh);
    LevelSim second{std::move(*fresh)};

    EXPECT_FALSE(second.result());
    second.start();
    second.advance(2 * dt);
    EXPECT_EQ(second.capital_ships().num_instances(), 2);
    EXPECT_EQ(second.turrets().num_instances(), 1);
}

TEST(LevelAuthoringIntegration, SpawnEventsPreserveOverridesAndResolveDelayedEntityReferences) {
    auto definition{ioj::levels::authoring::tests::read_test_level(mission_reader(), R"(
      (level :id 'authored-spawns :title "Authored spawns"
        :settings (module-ref (import-module "Settings/base.scm") 'game-settings)
        :camera (camera :look-at '(capital) :offset-direction '(-1 0 0) :distance 1000)
        :entities (list
          (entity :id 'capital :archetype 'capital-ship :team 'blue
            :position '(-20000 0 0) :health 345 :initial-spawn-delay 60 :spawn-cooldown 75)
          (entity :id 'spinner :archetype 'tube-spinner :team 'white
            :position '(0 30000 0) :rotation '(0 45 0) :initial-fire-point 1)
          (entity :id 'target :archetype 'static-turret :team 'red
            :position '(20000 0 0) :spawn-at 0.1 :health 234 :laser-damage 17))
        :logic (logic
          (on :when (entity-destroyed 'target) :do (win 'blue)))))")};
    ASSERT_TRUE(definition) << levels::format_diagnostics(definition.error());

    auto built{make_level_simulation_init_data(*definition, {}, {})};
    ASSERT_TRUE(built) << built.error().front();
    auto const capital{built->level_events.capital_spawns().const_view()};
    auto const turret{built->level_events.turret_spawns().const_view()};
    auto const spinner{built->level_events.spinner_spawns().const_view()};
    ASSERT_EQ(capital.num(), 1);
    ASSERT_EQ(turret.num(), 1);
    ASSERT_EQ(spinner.num(), 1);
    EXPECT_EQ(capital.entity_indices()[0], LevelEntityRef{0});
    EXPECT_EQ(spinner.entity_indices()[0], LevelEntityRef{1});
    EXPECT_EQ(turret.entity_indices()[0], LevelEntityRef{2});
    EXPECT_FALSE(LevelEntityRef{}.is_valid());
    EXPECT_EQ(capital.healths()[0], Health{345});
    EXPECT_EQ(capital.initial_fighter_spawn_delays()[0], 60.f);
    EXPECT_EQ(capital.fighter_spawn_cooldowns()[0], 75.f);
    EXPECT_EQ(turret.healths()[0], Health{234});
    EXPECT_EQ(turret.laser_damages()[0], 17);
    EXPECT_EQ(spinner.initial_fire_point_indices()[0], 1);

    LevelSim simulation{std::move(*built)};

    EXPECT_EQ(simulation.capital_ships().num_instances(), 1);
    ASSERT_EQ(simulation.spinners().num_instances(), 1);
    EXPECT_EQ(simulation.spinners().get_entities().yaws()[0], 45.f);
    EXPECT_EQ(simulation.spinners().get_entities().next_fire_point_indices()[0], 1);
    EXPECT_EQ(simulation.turrets().num_instances(), 0);

    simulation.start();
    auto const dt{simulation.clock().tick_period()};
    simulation.advance(dt);
    EXPECT_FALSE(simulation.result().has_value());
    simulation.advance(0.15);
    ASSERT_EQ(simulation.turrets().num_instances(), 1);
    EXPECT_FALSE(simulation.result().has_value());

    auto const victim{simulation.turrets().get_entities().entity_ids()[0]};
    auto const instigator{simulation.capital_ships().get_entities().entity_ids()[0]};
    DirectDamageEvents damage;
    damage.add(victim, 234, instigator);
    LevelSimTestAccess::queue_direct_damage_events(simulation, damage.const_view());
    simulation.advance(dt);
    EXPECT_EQ(simulation.turrets().num_instances(), 0);
    EXPECT_TRUE(simulation.result().has_value());
    EXPECT_TRUE(simulation.entity_ledger().is_valid_unique_id(victim));
}
}
