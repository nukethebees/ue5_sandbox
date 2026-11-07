#include "support/simulation_test_support.h"
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/sim/levels/level_simulation_builder.h>
#include <ioj/sim/testing/level_sim_test_access.h>

namespace ioj::sim::tests {
TEST(LevelAuthoringIntegration, SpawnEventsPreserveOverridesAndResolveDelayedObjectiveReferences) {
    auto definition{levels::authoring::DefinitionReader{}.read_level_source(R"(
      (level :id 'authored-spawns :title "Authored spawns"
        :camera (camera :look-at '(capital) :offset-direction '(-1 0 0) :distance 1000)
        :entities (list
          (entity :id 'capital :archetype 'capital-ship :team 'blue
            :position '(-20000 0 0) :health 345 :initial-spawn-delay 60 :spawn-cooldown 75)
          (entity :id 'spinner :archetype 'tube-spinner :team 'white
            :position '(0 30000 0) :rotation '(0 45 0) :initial-fire-point 1)
          (entity :id 'target :archetype 'static-turret :team 'red
            :position '(20000 0 0) :spawn-at 0.1 :health 234 :laser-damage 17))
        :mission (mission :objectives (list
          (objective :id 'destroy-target :team 'blue :wins-level 'yes
            :pass (entity-destroyed 'target))))))")};
    ASSERT_TRUE(definition) << levels::format_diagnostics(definition.error());

    auto built{make_level_simulation_init_data(*definition, make_simulation_data(make_fixture()))};
    ASSERT_TRUE(built) << built.error().front();
    auto const capital{built->level_events.capital_spawns.const_view()};
    auto const turret{built->level_events.turret_spawns.const_view()};
    auto const spinner{built->level_events.spinner_spawns.const_view()};
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
    simulation.finish_initialisation();
    EXPECT_EQ(simulation.capital_ships().num_instances(), 1);
    ASSERT_EQ(simulation.spinners().num_instances(), 1);
    EXPECT_EQ(simulation.spinners().get_entities().yaws()[0], 45.f);
    EXPECT_EQ(simulation.spinners().get_entities().next_fire_point_indices()[0], 1);
    EXPECT_EQ(simulation.turrets().num_instances(), 0);

    simulation.start();
    auto const dt{simulation.clock().tick_period()};
    simulation.advance(dt);
    EXPECT_FALSE(simulation.mission().mission_completed());
    simulation.advance(0.15);
    ASSERT_EQ(simulation.turrets().num_instances(), 1);
    EXPECT_FALSE(simulation.mission().mission_completed());

    auto const victim{simulation.turrets().get_entities().entity_ids()[0]};
    auto const instigator{simulation.capital_ships().get_entities().entity_ids()[0]};
    DirectDamageEvents damage;
    damage.add(victim, 234, instigator);
    LevelSimTestAccess::queue_direct_damage_events(simulation, damage.const_view());
    simulation.advance(dt);
    EXPECT_EQ(simulation.turrets().num_instances(), 0);
    EXPECT_TRUE(simulation.mission().mission_completed());
    EXPECT_TRUE(simulation.entity_ledger().is_valid_unique_id(victim));
}
}
