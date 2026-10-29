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
            auto check_level = [&](TOptional<FVector3f> const size,
                                   TOptional<FVector3f> const cells,
                                   TCHAR const* const label) {
                std::string grid;
                auto expected{base->collision_grid};
                if (size.IsSet()) {
                    auto const value{size.GetValue()};
                    grid += std::format(":level-size '({} {} {}) ", value.X, value.Y, value.Z);
                    expected.grid_size = value;
                }
                if (cells.IsSet()) {
                    auto const value{cells.GetValue()};
                    grid += std::format(":cell-size '({} {} {}) ", value.X, value.Y, value.Z);
                    expected.cell_size = value;
                }
                ioj::levels::authoring::LevelSource const source{std::format(
                    R"((level :id 'grid-runtime :title "Grid runtime" :player 'player
                        :entities (list (entity :id 'player :archetype 'player-fighter
                                                :team 'blue :position '(0 0 0)))
                        :settings (settings :collision-grid (collision-grid {}))))",
                    grid)};

                if (!checks.is_true(static_cast<bool>(orchestrator->load_level(source)), label)) {
                    return;
                }
                orchestrator->start_simulation();
                if (!checks.is_true(orchestrator->get_level_simulation() != nullptr,
                                    TEXT("LevelSim starts with the resolved grid"))) {
                    return;
                }

                auto const dimensions{expected.calculate_grid_dimensions()};
                auto const actual{orchestrator->get_spatial_query_manager().get_grid_geometry()};
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
            };

            check_level(NullOpt, NullOpt, TEXT("No grid override loads"));
            check_level(size_override, NullOpt, TEXT("Bounds-only override loads"));
            check_level(NullOpt, cell_override, TEXT("Cell-only override loads"));
            check_level(size_override, cell_override, TEXT("Both overrides load"));
            check_level(NullOpt, cell_override, TEXT("Sequential cell override loads"));
            check_level(size_override, NullOpt, TEXT("Next level inherits base cells"));
            check_level(NullOpt, NullOpt, TEXT("Final level inherits both base dimensions"));
        });
    }
};
