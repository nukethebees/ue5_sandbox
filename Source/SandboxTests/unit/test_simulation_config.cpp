#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/level_definition.h>
#include <ioj/sim/collision/entity_world_bounds.h>
#include <SandboxTests/support/SimulationTestAssets.h>
#include <SpaceGame/s7/GameSettingsReader.h>
#include <SpaceGame/simulation/LevelSimulationBuilder.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxCoreEngine/VectorConversion.h>

#include <Algo/Compare.h>
#include <CQTest.h>
#include <Engine/StaticMesh.h>
#include <Engine/StaticMeshActor.h>
#include <GameFramework/Actor.h>
#include <Misc/Paths.h>
#include <UObject/Package.h>

#include <filesystem>

TEST_CLASS(SpaceGameLevelConfig, "Sandbox.UnitTests")
{
    TEST_METHOD(RotatedWorldBoundsEncloseTransformedCorners)
    {
        ::ioj::sim::collision::EntityAABBs bounds{};
        auto const type{::ioj::sim::EntityType::CapitalShip};
        bounds.set_centre(type, {30.f, -10.f, 0.f});
        bounds.set_half_extents(type, {100.f, 20.f, 5.f});
        FVector3f const position{400.f, -200.f, 100.f};
        for (auto const rotation :
             {FRotator3f::ZeroRotator, FRotator3f{0.f, 90.f, 0.f}, FRotator3f{23.f, 47.f, -16.f}}) {
            FBox3f expected{ForceInit};
            auto const centre{ioj::to_unreal(bounds.centre(type))};
            auto const extent{bounds.half_extents(type)};
            for (int32 corner{}; corner < 8; ++corner) {
                FVector3f const offset{(corner & 1) ? extent.x() : -extent.x(),
                                       (corner & 2) ? extent.y() : -extent.y(),
                                       (corner & 4) ? extent.z() : -extent.z()};
                expected += position + rotation.RotateVector(centre + offset);
            }
            auto const orientation{rotation.Quaternion()};
            auto const native_bounds{::ioj::sim::collision::make_entity_world_bounds(
                bounds, type, ioj::Vector3f{position}, ioj::Quaternion4f{orientation})};
            FBox3f const actual{ioj::to_unreal(native_bounds.min),
                                ioj::to_unreal(native_bounds.max)};
            TestRunner->TestTrue(TEXT("World bounds match independently transformed corners"),
                                 actual.Min.Equals(expected.Min, 0.001f) &&
                                     actual.Max.Equals(expected.Max, 0.001f));
        }
    }

    TEST_METHOD(PrebuiltLevelsPreserveResolvedSettings)
    {
        auto const* const resources{ioj::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Default level config loads"), resources)) {
            return;
        }

        ioj::levels::LevelDefinitionComponents input{
            .metadata = {.id = ioj::levels::LevelId{"prebuilt"}, .title = "Prebuilt"},
            .player_entity_id = ioj::levels::EntityId{"player"},
            .entities = {{.id = ioj::levels::EntityId{"player"},
                          .archetype = ioj::levels::EntityArchetype::PlayerFighter,
                          .team = ioj::Team::Blue}}};
        input.settings.fighters.speed = 1234.f;
        input.settings.collision_grid.level_size = {2400000.f, 2000000.f, 500000.f};
        input.settings.collision_grid.cell_size = {6000.f, 5000.f, 20000.f};
        auto const definition{ioj::levels::LevelDefinition::make(input)};
        if (!TestRunner->TestTrue(TEXT("Prebuilt level is valid"), definition.has_value())) {
            return;
        }

        auto const loaded{
            prepare_level_resources(definition->settings(), *resources, GetTransientPackage())};
        if (!TestRunner->TestTrue(loaded ? TEXT("Level resources prepare") : *loaded.error(),
                                  loaded.has_value())) {
            return;
        }
        TestRunner->TestEqual(TEXT("Runtime uses resolved fighter speed"),
                              (*loaded)->fighters.simulation.speed,
                              input.settings.fighters.speed);
        TestRunner->TestTrue(TEXT("Runtime uses resolved collision dimensions"),
                             (*loaded)->collision_grid.grid_size ==
                                 ioj::to_unreal(input.settings.collision_grid.level_size));
        TestRunner->TestTrue(TEXT("Runtime uses resolved collision cells"),
                             (*loaded)->collision_grid.cell_size ==
                                 ioj::to_unreal(input.settings.collision_grid.cell_size));
    }

    TEST_METHOD(DuplicatePreservesNestedValuesAndAssetReferences)
    {
        auto const* const source{ioj::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Default level config loads"), source)) {
            return;
        }
        if (!TestRunner->TestTrue(TEXT("Default level config is valid"), source->is_valid())) {
            return;
        }

        auto* const copy{ioj::duplicate_level_config(*source, *GetTransientPackage())};
        if (!TestRunner->TestNotNull(TEXT("Level config copy is created"), copy)) {
            return;
        }

        TestRunner->TestTrue(TEXT("Copy has a different address"), source != copy);
        TestRunner->TestTrue(TEXT("Copied level config is valid"), copy->is_valid());
        TestRunner->TestEqual(TEXT("Player laser damage is preserved"),
                              copy->player_ship.simulation.laser.damage,
                              source->player_ship.simulation.laser.damage);
        TestRunner->TestEqual(TEXT("Fighter spawn transforms are preserved"),
                              copy->capital_ships.fighter_spawn_slots_relative_transforms.Num(),
                              source->capital_ships.fighter_spawn_slots_relative_transforms.Num());
        TestRunner->TestEqual(TEXT("Turret projectile speed is preserved"),
                              copy->turrets.simulation.laser.projectile_speed.get(),
                              source->turrets.simulation.laser.projectile_speed.get());
        TestRunner->TestTrue(TEXT("Player team visual remains shared"),
                             copy->player_ship.team_visual_data ==
                                 source->player_ship.team_visual_data);
        TestRunner->TestTrue(TEXT("Projectile mesh remains shared"),
                             copy->laser_projectiles.mesh == source->laser_projectiles.mesh);
        TestRunner->TestTrue(TEXT("Capital mesh remains shared"),
                             copy->capital_ships.mesh == source->capital_ships.mesh);
        TestRunner->TestTrue(TEXT("Fighter mesh remains shared"),
                             copy->fighters.mesh == source->fighters.mesh);
        TestRunner->TestTrue(TEXT("Turret mesh remains shared"),
                             copy->turrets.mesh == source->turrets.mesh);
        TestRunner->TestTrue(TEXT("Spinner mesh remains shared"),
                             copy->tube_spinners.mesh == source->tube_spinners.mesh);
        TestRunner->TestTrue(TEXT("Collision-grid size is preserved"),
                             copy->collision_grid.grid_size == source->collision_grid.grid_size);
        TestRunner->TestTrue(TEXT("Collision-grid cell size is preserved"),
                             copy->collision_grid.cell_size == source->collision_grid.cell_size);
    }

    TEST_METHOD(FighterSpawnSlotsHaveClearance)
    {
        auto const* const config{ioj::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Default level config loads"), config) ||
            !TestRunner->TestNotNull(TEXT("Fighter mesh loads"), config->fighters.mesh.Get())) {
            return;
        }

        auto const& transforms{config->capital_ships.fighter_spawn_slots_relative_transforms};
        auto const clearance_radius{config->fighters.mesh->GetBounds().SphereRadius +
                                    config->fighters.simulation.avoidance_clearance_buffer};
        auto minimum_distance{TNumericLimits<float>::Max()};
        for (int32 i{}; i < transforms.Num(); ++i) {
            for (int32 j{i + 1}; j < transforms.Num(); ++j) {
                minimum_distance = FMath::Min(
                    minimum_distance,
                    FVector::Dist(transforms[i].GetLocation(), transforms[j].GetLocation()));
            }
        }

        TestRunner->TestTrue(TEXT("Authored fighter spawn slots have collision clearance"),
                             minimum_distance >= clearance_radius * 2.f);

        auto const result{ioj::make_level_simulation_init_data(*config)};
        TestRunner->TestTrue(TEXT("Authored fighter spawn slots pass level-start validation"),
                             result.has_value());
    }

    TEST_METHOD(RuntimeConfigStartsTurretTrialZero)
    {
        auto const* const source{ioj::load_default_level_config()};
        auto const* const runtime{
            LoadObject<USpaceGameLevelConfig>(nullptr,
                                              TEXT("/SpaceGame/Levels/DA_GameRuntimeLevelConfig."
                                                   "DA_GameRuntimeLevelConfig"))};
        if (!TestRunner->TestNotNull(TEXT("Source level config loads"), source) ||
            !TestRunner->TestNotNull(TEXT("Runtime level config loads"), runtime)) {
            return;
        }

        auto const& runtime_transforms{
            runtime->capital_ships.fighter_spawn_slots_relative_transforms};
        auto const& source_transforms{
            source->capital_ships.fighter_spawn_slots_relative_transforms};
        TestRunner->TestTrue(
            TEXT("Runtime fighter spawn transforms match their source"),
            Algo::Compare(runtime_transforms, source_transforms, [](auto const& a, auto const& b) {
                return a.Equals(b);
            }));

        ::ioj::levels::authoring::DefinitionReader reader;
        auto const script_path{
            FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"), TEXT("TurretTrial_0.scm"))};
        auto const scripted_definition{reader.read_level_file(std::filesystem::path{*script_path})};
        if (!TestRunner->TestTrue(TEXT("Turret Trial 0 produces a valid native definition"),
                                  static_cast<bool>(scripted_definition))) {
            return;
        }

        auto const result{
            ioj::make_level_simulation_init_data(*runtime, {}, scripted_definition.value())};
        TestRunner->TestTrue(TEXT("Runtime config starts Turret Trial 0"), result.has_value());
    }

    TEST_METHOD(RejectsOverlappingStaticCollisionClassLists)
    {
        auto const* const source{ioj::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Default level config loads"), source)) {
            return;
        }

        auto* const copy{ioj::duplicate_level_config(*source, *GetTransientPackage())};
        if (!TestRunner->TestNotNull(TEXT("Level config copy is created"), copy)) {
            return;
        }

        copy->collision_grid.harvested_collision_actor_classes = {AStaticMeshActor::StaticClass()};
        copy->collision_grid.omitted_collision_actor_classes = {AActor::StaticClass()};

        TArray<FString> errors;
        copy->get_validation_errors(errors);
        TestRunner->TestFalse(TEXT("Inherited class-list overlap is invalid"), copy->is_valid());
        TestRunner->TestTrue(
            TEXT("Class-list overlap has a validation diagnostic"),
            errors.Contains(
                TEXT("collision_grid static collision actor class lists are invalid or overlap")));
        TestRunner->TestFalse(
            TEXT("Class-list overlap does not report a grid-dimensions error"),
            errors.Contains(
                TEXT("collision_grid calculated dimensions and cell count must fit in int32")));
    }
};
