#include <ioj/sim/spatial_query_manager.h>
#include <SandboxTests/support/test_setup.h>
#include <SpaceGame/levels/ExampleLevels.h>
#include <SpaceGame/levels/LevelLoader.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <CQTest.h>

TEST_CLASS(S7CollisionGridRuntime, "Sandbox.LevelTests")
{
    inline static ml::FTestBatchOrchestratorLevelSetup level_setup{};
    ml::FSoftTestAssertions checks{};

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
            auto* const base{
                DuplicateObject<USpaceGameLevelConfig>(&level_setup.get_config(), orchestrator)};
            if (!checks.is_true(IsValid(base), TEXT("Base config duplicates"))) {
                return;
            }
            base->collision_grid.grid_size = FVector3f{2150000.f, 2050000.f, 430000.f};
            base->collision_grid.cell_size = FVector3f{37000.f, 43000.f, 27000.f};
            orchestrator->set_level_config(*base);

            auto const size_override{FVector3f{2450000.f, 2250000.f, 550000.f}};
            auto const cell_override{FVector3f{47000.f, 53000.f, 35000.f}};
            auto check_level = [&](TOptional<FVector3f> const size,
                                   TOptional<FVector3f> const cells,
                                   TCHAR const* const label) {
                auto definition{ml::example_levels::make_native_example()};
                if (size.IsSet() || cells.IsSet()) {
                    definition.collision_grid =
                        ml::FLevelCollisionGridDefinition{.level_size = size, .cell_size = cells};
                } else {
                    definition.collision_grid = NullOpt;
                }

                ml::FLevelLoader loader{*orchestrator};
                if (!checks.is_true(static_cast<bool>(loader.load(definition)), label)) {
                    return;
                }
                orchestrator->start_simulation();
                if (!checks.is_true(orchestrator->get_level_simulation() != nullptr,
                                    TEXT("LevelSim starts with the resolved grid"))) {
                    return;
                }

                auto const expected{base->collision_grid.with_dimension_overrides(size, cells)};
                auto const dimensions{expected.calculate_grid_dimensions()};
                auto const actual{orchestrator->get_spatial_query_manager().get_grid_geometry()};
                checks.is_true(actual.dimensions.x == dimensions.X &&
                                   actual.dimensions.y == dimensions.Y &&
                                   actual.dimensions.z == dimensions.Z,
                               TEXT("LevelSim grid bounds come from base plus current override"));
                checks.is_true(actual.cell_dimensions.X == expected.cell_size.X &&
                                   actual.cell_dimensions.Y == expected.cell_size.Y &&
                                   actual.cell_dimensions.Z == expected.cell_size.Z,
                               TEXT("LevelSim cell size comes from base plus current override"));
                checks.is_true(orchestrator->get_level_config() == base,
                               TEXT("Effective config never replaces the base asset"));
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
