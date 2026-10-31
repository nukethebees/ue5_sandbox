#include <ioj/sim/collision/spatial_query_manager.h>
#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/test_setup.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <CQTest.h>

#include <format>

TEST_CLASS(S7CollisionGridRuntime, "Sandbox.LevelTests")
{
    inline static ioj::FTestBatchOrchestratorLevelSetup level_setup{};
    ioj::FSoftTestAssertions checks{};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        level_setup.begin_test(TestCommandBuilder, *TestRunner, checks);
    }

    AFTER_EACH()
    { level_setup.end_test(); }

    AFTER_ALL()
    { level_setup.teardown(); }

    TEST_METHOD(EffectiveGridUsesOnlyBaseAndCurrentLevelOverrides)
    {
        TestCommandBuilder.Do([this] {
            auto* const orchestrator{level_setup.get_orchestrator()};
            if (!checks.is_valid(orchestrator, TEXT("Runtime orchestrator exists"))) {
                return;
            }
            auto* const base{ioj::duplicate_level_config(level_setup.get_config(), *orchestrator)};
            if (!checks.is_true(IsValid(base), TEXT("Base config duplicates"))) {
                return;
            }
            orchestrator->set_level_config(*base);

            auto const size_override{FVector3f{2450000.f, 2250000.f, 550000.f}};
            auto const cell_override{FVector3f{47000.f, 53000.f, 35000.f}};
            for (auto const override_grid : {true, false}) {
                auto expected{base->collision_grid};
                std::string grid;
                if (override_grid) {
                    grid = std::format(":level-size '({} {} {}) :cell-size '({} {} {})",
                                       size_override.X,
                                       size_override.Y,
                                       size_override.Z,
                                       cell_override.X,
                                       cell_override.Y,
                                       cell_override.Z);
                    expected.grid_size = size_override;
                    expected.cell_size = cell_override;
                }
                ioj::levels::LevelSource const source{std::format(
                    R"((level :id 'grid-runtime :title "Grid runtime" :player 'player
                        :entities (list (entity :id 'player :archetype 'player-fighter
                                                :team 'blue :position '(0 0 0)))
                        :settings (settings :collision-grid (collision-grid {}))))",
                    grid)};

                if (!checks.is_true(static_cast<bool>(orchestrator->load_level(source)),
                                    TEXT("Current level grid loads"))) {
                    return;
                }
                orchestrator->start_simulation();
                if (!checks.is_true(orchestrator->get_level_simulation() != nullptr,
                                    TEXT("LevelSim starts with the resolved grid"))) {
                    return;
                }

                auto const dimensions{expected.calculate_grid_dimensions()};
                auto const actual{orchestrator->get_level_simulation()
                                      ->get_spatial_query_manager()
                                      .get_grid_geometry()};
                checks.is_true(actual.dimensions == ioj::sim::collision::CellCoord{dimensions.X,
                                                                                   dimensions.Y,
                                                                                   dimensions.Z},
                               TEXT("LevelSim grid bounds come from base plus current override"));
                checks.is_true(actual.cell_dimensions == ioj::Vector3f{expected.cell_size},
                               TEXT("LevelSim cell size comes from base plus current override"));
                checks.is_true(orchestrator->get_level_config() != base,
                               TEXT("Orchestrator owns the resolved settings"));
                auto* const player{const_cast<ATestSpaceShip*>(orchestrator->get_player_ship())};
                if (IsValid(player)) {
                    player->Destroy();
                }
                orchestrator->clear_player_ship();
                orchestrator->reset_for_new_level();
            }
        });
    }
};
