#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/sim/collision/entity_world_bounds.h>
#include <SandboxTests/support/SimulationTestAssets.h>
#include <SpaceGame/s7/GameSettingsReader.h>
#include <SpaceGame/simulation/LevelSimulationBuilder.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGameS7/definition_reader.h>

#include <CQTest.h>
#include <Engine/StaticMesh.h>
#include <Engine/StaticMeshActor.h>
#include <GameFramework/Actor.h>
#include <Misc/Paths.h>
#include <UObject/Package.h>

#include <format>
#include <limits>

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
            auto const native_centre{bounds.get_centre(type)};
            FVector3f const centre{native_centre.x(), native_centre.y(), native_centre.z()};
            auto const extent{bounds.get_half_extents(type)};
            for (int32 corner{}; corner < 8; ++corner) {
                FVector3f const offset{(corner & 1) ? extent.x() : -extent.x(),
                                       (corner & 2) ? extent.y() : -extent.y(),
                                       (corner & 4) ? extent.z() : -extent.z()};
                expected += position + rotation.RotateVector(centre + offset);
            }
            auto const orientation{rotation.Quaternion()};
            auto const native_bounds{::ioj::sim::collision::make_entity_world_bounds(
                bounds,
                type,
                ioj::Vector3f{position.X, position.Y, position.Z},
                ioj::Quaternion4f{orientation.X, orientation.Y, orientation.Z, orientation.W})};
            FBox3f const actual{
                FVector3f{native_bounds.min.x(), native_bounds.min.y(), native_bounds.min.z()},
                FVector3f{native_bounds.max.x(), native_bounds.max.y(), native_bounds.max.z()}};
            TestRunner->TestTrue(TEXT("World bounds match independently transformed corners"),
                                 actual.Min.Equals(expected.Min, 0.001f) &&
                                     actual.Max.Equals(expected.Max, 0.001f));
        }
    }

    TEST_METHOD(CollisionGridDefaultsAndValidation)
    {
        FCollisionGridConfig const defaults{};
        TestRunner->TestTrue(TEXT("Default collision-grid size is 20 km by 20 km by 1 km"),
                             defaults.grid_size == FVector3f{2000000.f, 2000000.f, 100000.f});
        TestRunner->TestTrue(TEXT("Default collision-grid cell size is preserved"),
                             defaults.cell_size == FVector3f{5000.f, 5000.f, 20000.f});
        TestRunner->TestTrue(TEXT("Default collision-grid dimensions are calculated"),
                             defaults.calculate_grid_dimensions() == FIntVector3{400, 400, 5});
        TestRunner->TestTrue(TEXT("Default collision-grid config is valid"), defaults.is_valid());

        auto const* const source{ioj::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Default level config loads"), source)) {
            return;
        }

        auto* const copy{ioj::duplicate_level_config(*source, *GetTransientPackage())};
        if (!TestRunner->TestNotNull(TEXT("Level config copy is created"), copy)) {
            return;
        }

        TestRunner->TestTrue(TEXT("Collision-grid size is duplicated"),
                             copy->collision_grid.grid_size == source->collision_grid.grid_size);
        TestRunner->TestTrue(TEXT("Collision-grid cell size is duplicated"),
                             copy->collision_grid.cell_size == source->collision_grid.cell_size);
        TestRunner->TestTrue(TEXT("Collision-grid calculated dimensions are duplicated"),
                             copy->collision_grid.calculate_grid_dimensions() ==
                                 source->collision_grid.calculate_grid_dimensions());

        copy->collision_grid.grid_size.X = 0.f;
        TArray<FString> errors;
        copy->get_validation_errors(errors);
        TestRunner->TestFalse(TEXT("Zero collision-grid size is invalid"), copy->is_valid());
        TestRunner->TestTrue(
            TEXT("Invalid collision-grid size has a validation diagnostic"),
            errors.Contains(TEXT("collision_grid.grid_size components must be positive")));
    }

    TEST_METHOD(PresentationVectorsRejectNonfiniteAndUnrepresentableComponents)
    {
        auto const* const resources{ioj::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Default level config loads"), resources)) {
            return;
        }
        auto load = [&](std::string_view const path,
                        std::vector<double> values) -> std::expected<void, FString> {
            std::string components;
            for (double value : values) {
                components += std::isnan(value) ? "+nan.0 "
                            : std::isinf(value) ? "+inf.0 "
                                                : std::format("{} ", value);
            }
            auto const overlay{path.find("entity-overlay") != std::string_view::npos};
            auto const source{std::format(R"((level :id 'vector-settings :title "Vector settings"
                  :player 'player :entities (list
                    (entity :id 'player :archetype 'player-fighter :team 'blue :position '(0 0 0)))
                  :settings (settings :presentation (presentation :{} (settings-section '{} :{} '({}))))))",
                                          overlay ? "entity-overlay" : "capital-ships",
                                          overlay ? "entity-overlay" : "capital-ships",
                                          overlay ? "bar-size-pixels" : "min-small-explosion-range",
                                          components)};
            ioj::levels::authoring::LevelSource input{source};
            auto loaded{read_game_settings(
                TEXT("LevelScripts/Settings/base.scm"), &input, *resources, GetTransientPackage())};
            if (loaded) {
                TestRunner->TestEqual(TEXT("Presentation-only settings retain base fighter speed"),
                                      loaded->simulation.fighters.speed,
                                      4500.f);
                TestRunner->TestEqual(TEXT("Resolved definition uses the same base settings"),
                                      loaded->level->settings()->fighters.speed,
                                      loaded->simulation.fighters.speed);
            }
            return loaded.transform([](auto const&) {});
        };
        for (auto const path : {"presentation.entity-overlay.bar-size-pixels",
                                "presentation.capital-ships.min-small-explosion-range"}) {
            auto const count{
                std::string_view{path}.find("entity-overlay") != std::string_view::npos ? 2u : 3u};
            auto const valid{load(path, std::vector<double>(count, 2.5))};
            TestRunner->TestTrue(valid ? TEXT("Finite vector components are accepted")
                                       : *valid.error(),
                                 valid.has_value());
            for (auto const invalid : {std::numeric_limits<double>::infinity(),
                                       std::numeric_limits<double>::quiet_NaN()}) {
                for (std::size_t index{}; index < count; ++index) {
                    std::vector<double> values(count, 2.5);
                    values[index] = invalid;
                    TestRunner->TestFalse(TEXT("Nonfinite vector component is rejected"),
                                          load(path, std::move(values)).has_value());
                }
            }
        }
        TestRunner->TestFalse(TEXT("Unrepresentable FVector3f component is rejected"),
                              load("presentation.capital-ships.min-small-explosion-range",
                                   {1., 2., std::numeric_limits<double>::max()})
                                  .has_value());
        TestRunner->TestTrue(TEXT("FVector2D retains double precision range"),
                             load("presentation.entity-overlay.bar-size-pixels",
                                  {1., std::numeric_limits<double>::max()})
                                 .has_value());
    }

    TEST_METHOD(CollisionGridDimensionOverridesKeepUnspecifiedAssetValues)
    {
        FCollisionGridConfig source;
        source.grid_size = FVector3f{2000000.f, 2000000.f, 500000.f};
        source.cell_size = FVector3f{5000.f, 5000.f, 20000.f};

        auto const inherited{source.with_dimension_overrides(NullOpt, NullOpt)};
        TestRunner->TestTrue(TEXT("No overrides preserve asset dimensions"),
                             inherited.grid_size == source.grid_size &&
                                 inherited.cell_size == source.cell_size);

        auto const authored_cells{
            source.with_dimension_overrides(NullOpt, FVector3f{6000.f, 6000.f, 20000.f})};
        TestRunner->TestTrue(TEXT("Cell override keeps asset size"),
                             authored_cells.grid_size == source.grid_size);
        TestRunner->TestTrue(TEXT("Cell override applies independently"),
                             authored_cells.cell_size == FVector3f{6000.f, 6000.f, 20000.f});

        source.grid_size.X += 10000.f;
        auto const updated{
            source.with_dimension_overrides(NullOpt, FVector3f{6000.f, 6000.f, 20000.f})};
        TestRunner->TestTrue(TEXT("Inherited size follows changed asset"),
                             updated.grid_size == source.grid_size);
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
                              copy->turrets.simulation.laser.projectile_speed,
                              source->turrets.simulation.laser.projectile_speed);
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
        auto transforms_match{runtime_transforms.Num() == source_transforms.Num()};
        for (int32 i{}; transforms_match && i < runtime_transforms.Num(); ++i) {
            transforms_match = runtime_transforms[i].Equals(source_transforms[i]);
        }
        TestRunner->TestTrue(TEXT("Runtime fighter spawn transforms match their source"),
                             transforms_match);

        ::ioj::levels::authoring::FDefinitionReader reader;
        auto const script_path{
            FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"), TEXT("TurretTrial_0.scm"))};
        auto const scripted_definition{reader.read_level_file(script_path)};
        if (!TestRunner->TestTrue(TEXT("Turret Trial 0 produces a valid native definition"),
                                  static_cast<bool>(scripted_definition))) {
            return;
        }

        auto const result{
            ioj::make_level_simulation_init_data(*runtime, {}, scripted_definition.value())};
        TestRunner->TestTrue(TEXT("Runtime config starts Turret Trial 0"), result.has_value());
    }

    TEST_METHOD(FighterSpawnSlotValidationRejectsInvalidLevelGeometry)
    {
        auto const* const source{ioj::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Default level config loads"), source) ||
            !TestRunner->TestNotNull(TEXT("Capital mesh loads"),
                                     source->capital_ships.mesh.Get())) {
            return;
        }

        auto* const inside_capital{ioj::duplicate_level_config(*source, *GetTransientPackage())};
        inside_capital->capital_ships.fighter_spawn_slots_relative_transforms[0].SetLocation(
            source->capital_ships.mesh->GetBounds().Origin);
        auto const capital_result{ioj::make_level_simulation_init_data(*inside_capital)};
        TestRunner->TestFalse(TEXT("A fighter slot inside its capital aborts level loading"),
                              capital_result.has_value());
        if (!capital_result) {
            TestRunner->TestTrue(
                TEXT("Capital intersection has a level-start diagnostic"),
                capital_result.error().format().Contains(TEXT("intersects the capital collision")));
        }

        auto* const overlapping_fighters{
            ioj::duplicate_level_config(*source, *GetTransientPackage())};
        overlapping_fighters->capital_ships.fighter_spawn_slots_relative_transforms[1].SetLocation(
            overlapping_fighters->capital_ships.fighter_spawn_slots_relative_transforms[0]
                .GetLocation());
        auto const fighter_result{ioj::make_level_simulation_init_data(*overlapping_fighters)};
        TestRunner->TestFalse(TEXT("Overlapping fighter slots abort level loading"),
                              fighter_result.has_value());
        if (!fighter_result) {
            TestRunner->TestTrue(
                TEXT("Fighter overlap has a level-start diagnostic"),
                fighter_result.error().format().Contains(TEXT("fighter spawn slots 0 and 1")));
        }
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
