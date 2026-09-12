#include <SandboxTests/support/SimulationTestAssets.h>

#include <SandboxCore/soa_rotator_utils.h>
#include <SpaceGame/combat/lasers/TestLasersConfig.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnersConfig.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsConfig.h>
#include <SpaceGame/ships/capital/TestCapitalShipsConfig.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersConfig.h>
#include <SpaceGame/ships/player/TestSpaceShipData.h>
#include <SpaceGame/simulation/LevelSimulationBuilder.h>
#include <SpaceGame/simulation/SimulationConfig.h>
#include <SpaceGame/simulation/SimulationConfigConversion.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestSimulationConfig.h>
#include <SpaceGameS7/LevelDefinitionReader.h>
#include <SpaceGameSimulation/simulation/EntityWorldBounds.h>

#include <CQTest.h>
#include <Engine/StaticMesh.h>
#include <Engine/StaticMeshActor.h>
#include <GameFramework/Actor.h>
#include <Misc/Paths.h>
#include <UObject/Package.h>
#include <UObject/SoftObjectPtr.h>

TEST_CLASS(SpaceGameLevelConfig, "Sandbox.UnitTests")
{
    TEST_METHOD(PlanarMovementDefaults)
    {
        FPlayerShipConfig const presentation_defaults{};
        FPlayerSimulationConfig const simulation_defaults{};
        TestRunner->TestEqual(TEXT("Presentation lateral trim defaults to 3000"),
                              presentation_defaults.planar_lateral_trim_speed,
                              3000.f);
        TestRunner->TestEqual(TEXT("Presentation vertical trim defaults to 3000"),
                              presentation_defaults.planar_vertical_trim_speed,
                              3000.f);
        TestRunner->TestEqual(TEXT("Simulation lateral trim defaults to 3000"),
                              simulation_defaults.planar_lateral_trim_speed,
                              3000.f);
        TestRunner->TestEqual(TEXT("Simulation vertical trim defaults to 3000"),
                              simulation_defaults.planar_vertical_trim_speed,
                              3000.f);
        TestRunner->TestEqual(TEXT("Presentation forward velocity trim defaults to five percent"),
                              presentation_defaults.forward_velocity_trim_fraction,
                              0.05f);
        TestRunner->TestEqual(TEXT("Simulation forward velocity trim defaults to five percent"),
                              simulation_defaults.forward_velocity_trim_fraction,
                              0.05f);
        auto const converted_defaults{make_simulation_config(presentation_defaults)};
        TestRunner->TestEqual(TEXT("Forward velocity trim is copied into simulation config"),
                              converted_defaults.forward_velocity_trim_fraction,
                              presentation_defaults.forward_velocity_trim_fraction);

        auto const* const source{ml::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Default level config loads"), source)) {
            return;
        }
        TestRunner->TestEqual(TEXT("Authored lateral trim uses the current default"),
                              source->player_ship.planar_lateral_trim_speed,
                              3000.f);
        TestRunner->TestEqual(TEXT("Authored vertical trim uses the current default"),
                              source->player_ship.planar_vertical_trim_speed,
                              3000.f);
        TestRunner->TestEqual(TEXT("Authored forward velocity trim uses the current default"),
                              source->player_ship.forward_velocity_trim_fraction,
                              0.05f);
    }

    TEST_METHOD(RotatedWorldBoundsEncloseTransformedCorners)
    {
        ml::ioj::FEntityAABBs bounds{};
        auto const index{ml::ioj::FEntityAABBs::capital_ship_index};
        bounds.centre_xs[index] = 30.f;
        bounds.centre_ys[index] = -10.f;
        bounds.half_extent_xs[index] = 100.f;
        bounds.half_extent_ys[index] = 20.f;
        bounds.half_extent_zs[index] = 5.f;
        FVector3f const position{400.f, -200.f, 100.f};
        for (auto const rotation :
             {FRotator3f::ZeroRotator, FRotator3f{0.f, 90.f, 0.f}, FRotator3f{23.f, 47.f, -16.f}}) {
            FBox3f expected{ForceInit};
            auto const centre{bounds.get_centre(index)};
            auto const extent{bounds.get_half_extents(index)};
            for (int32 corner{}; corner < 8; ++corner) {
                FVector3f const offset{(corner & 1) ? extent.X : -extent.X,
                                       (corner & 2) ? extent.Y : -extent.Y,
                                       (corner & 4) ? extent.Z : -extent.Z};
                expected += position + rotation.RotateVector(centre + offset);
            }
            auto const actual{ml::ioj::to_unreal(
                ml::ioj::make_entity_world_bounds(bounds, index, position, rotation))};
            TestRunner->TestTrue(TEXT("World bounds match independently transformed corners"),
                                 actual.Min.Equals(expected.Min, 0.001f) &&
                                     actual.Max.Equals(expected.Max, 0.001f));
        }
    }

    TEST_METHOD(RotatedCapitalSpawnClearance)
    {
        FLevelSimulationInitData data;
        auto const index{ml::ioj::FEntityAABBs::capital_ship_index};
        data.entity_bounds.half_extent_xs[index] = 100.f;
        data.entity_bounds.half_extent_ys[index] = 20.f;
        data.entity_bounds.half_extent_zs[index] = 10.f;
        data.fighter_radius = 5.f;
        data.fighters.avoidance_clearance_buffer = 1.f;
        data.capital_ships.fighter_spawn_slots_relative_transforms = {
            FTransform{FVector{0.f, 40.f, 0.f}}};
        data.capital_spawns.add_defaulted(1);
        ml::FLevelStartErrors clear_errors;
        ml::validate_world_fighter_spawn_slots(data, clear_errors);
        TestRunner->TestFalse(TEXT("Unrotated slot clears capital"), clear_errors.has_errors());
        ml::assign(data.capital_spawns.rotations, 0, FRotator3f{0.f, 45.f, 0.f});
        ml::FLevelStartErrors rotated_errors;
        ml::validate_world_fighter_spawn_slots(data, rotated_errors);
        TestRunner->TestTrue(
            TEXT("Rotation can place valid local slot inside conservative world AABB"),
            rotated_errors.has_errors());
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

        auto const* const source{ml::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Default level config loads"), source)) {
            return;
        }

        auto* const copy{DuplicateObject<USpaceGameLevelConfig>(source, GetTransientPackage())};
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

    TEST_METHOD(DuplicatePreservesNestedValuesAndAssetReferences)
    {
        auto const* const source{ml::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Default level config loads"), source)) {
            return;
        }
        if (!TestRunner->TestTrue(TEXT("Default level config is valid"), source->is_valid())) {
            return;
        }

        auto* const copy{DuplicateObject<USpaceGameLevelConfig>(source, GetTransientPackage())};
        if (!TestRunner->TestNotNull(TEXT("Level config copy is created"), copy)) {
            return;
        }

        TestRunner->TestTrue(TEXT("Copy has a different address"), source != copy);
        TestRunner->TestTrue(TEXT("Copied level config is valid"), copy->is_valid());
        TestRunner->TestEqual(TEXT("Player laser damage is preserved"),
                              copy->player_ship.laser.damage,
                              source->player_ship.laser.damage);
        TestRunner->TestEqual(TEXT("Fighter spawn transforms are preserved"),
                              copy->capital_ships.fighter_spawn_slots_relative_transforms.Num(),
                              source->capital_ships.fighter_spawn_slots_relative_transforms.Num());
        TestRunner->TestEqual(TEXT("Turret projectile speed is preserved"),
                              copy->turrets.laser.projectile_speed,
                              source->turrets.laser.projectile_speed);
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

    TEST_METHOD(FighterNavigationDefaultsAndValidation)
    {
        FFighterConfig const defaults{};
        TestRunner->TestEqual(TEXT("Clear navigation defaults to 2 Hz"),
                              defaults.avoidance_clear_update_frequency,
                              2.f);
        TestRunner->TestEqual(TEXT("Nearby navigation preserves the 5 Hz default"),
                              defaults.avoidance_update_frequency,
                              5.f);
        TestRunner->TestEqual(TEXT("Active navigation defaults to 12 Hz"),
                              defaults.avoidance_active_update_frequency,
                              12.f);
        TestRunner->TestEqual(TEXT("Immediate navigation defaults to 30 Hz"),
                              defaults.avoidance_immediate_update_frequency,
                              30.f);
        TestRunner->TestEqual(
            TEXT("Fighter separation defaults to 2000 cm"), defaults.separation_radius, 2000.f);
        TestRunner->TestEqual(TEXT("Fighter steering memory defaults to 0.75 seconds"),
                              defaults.steering_memory_duration,
                              0.75f);
        TestRunner->TestEqual(TEXT("Dense traffic defaults to four neighbours"),
                              defaults.dense_traffic_neighbour_threshold,
                              4);

        auto const* const source{ml::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Default level config loads"), source)) {
            return;
        }
        auto* const copy{DuplicateObject<USpaceGameLevelConfig>(source, GetTransientPackage())};
        if (!TestRunner->TestNotNull(TEXT("Level config copy is created"), copy)) {
            return;
        }

        copy->fighters.avoidance_active_update_frequency =
            copy->fighters.avoidance_update_frequency * 0.5f;
        TArray<FString> errors;
        copy->get_validation_errors(errors);
        TestRunner->TestFalse(TEXT("Out-of-order navigation tiers are invalid"), copy->is_valid());
        TestRunner->TestTrue(
            TEXT("Invalid navigation tiers have a diagnostic"),
            errors.Contains(TEXT("fighter avoidance update frequencies must be non-decreasing by "
                                 "risk tier")));

        copy->fighters.avoidance_active_update_frequency = 12.f;
        copy->fighters.separation_radius = 0.f;
        errors.Reset();
        copy->get_validation_errors(errors);
        TestRunner->TestFalse(TEXT("Zero separation radius is invalid"), copy->is_valid());
        TestRunner->TestTrue(
            TEXT("Invalid separation radius has a diagnostic"),
            errors.Contains(TEXT("fighters.separation_radius must be finite and positive")));

        copy->fighters.separation_radius = 2000.f;
        copy->fighters.dense_traffic_neighbour_threshold = 1;
        errors.Reset();
        copy->get_validation_errors(errors);
        TestRunner->TestFalse(TEXT("Single-neighbour dense traffic is invalid"), copy->is_valid());
        TestRunner->TestTrue(
            TEXT("Invalid dense-traffic threshold has a diagnostic"),
            errors.Contains(
                TEXT("fighters.dense_traffic_neighbour_threshold must be at least two")));
    }

    TEST_METHOD(FighterSpawnSlotsHaveClearance)
    {
        auto const* const config{ml::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Default level config loads"), config) ||
            !TestRunner->TestNotNull(TEXT("Fighter mesh loads"), config->fighters.mesh.Get())) {
            return;
        }

        auto const& transforms{config->capital_ships.fighter_spawn_slots_relative_transforms};
        auto const clearance_radius{config->fighters.mesh->GetBounds().SphereRadius +
                                    config->fighters.avoidance_clearance_buffer};
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

        auto const result{ml::make_level_simulation_init_data(*config)};
        TestRunner->TestTrue(TEXT("Authored fighter spawn slots pass level-start validation"),
                             result.has_value());
    }

    TEST_METHOD(RuntimeConfigStartsTurretTrialZero)
    {
        auto const* const source{ml::load_default_level_config()};
        auto const* const runtime{
            LoadObject<USpaceGameLevelConfig>(nullptr,
                                              TEXT("/SpaceGame/Levels/DA_GameRuntimeLevelConfig."
                                                   "DA_GameRuntimeLevelConfig"))};
        if (!TestRunner->TestNotNull(TEXT("Source level config loads"), source) ||
            !TestRunner->TestNotNull(TEXT("Runtime level config loads"), runtime)) {
            return;
        }

        TestRunner->TestEqual(TEXT("Runtime fighter spawn count matches its source"),
                              runtime->capital_ships.fighter_spawn_slots,
                              source->capital_ships.fighter_spawn_slots);
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

        ml::s7::FLevelDefinitionReader reader;
        auto const script_path{
            FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"), TEXT("TurretTrial_0.scm"))};
        auto const scripted_definition{reader.read_file(script_path)};
        if (!TestRunner->TestTrue(TEXT("Turret Trial 0 produces a valid native definition"),
                                  static_cast<bool>(scripted_definition))) {
            return;
        }

        auto const result{ml::make_level_simulation_init_data(
            *runtime, {}, scripted_definition.definition.GetValue())};
        TestRunner->TestTrue(TEXT("Runtime config starts Turret Trial 0"), result.has_value());
    }

    TEST_METHOD(FighterSpawnSlotValidationRejectsInvalidLevelGeometry)
    {
        auto const* const source{ml::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Default level config loads"), source) ||
            !TestRunner->TestNotNull(TEXT("Capital mesh loads"),
                                     source->capital_ships.mesh.Get())) {
            return;
        }

        auto* const inside_capital{
            DuplicateObject<USpaceGameLevelConfig>(source, GetTransientPackage())};
        inside_capital->capital_ships.fighter_spawn_slots_relative_transforms[0].SetLocation(
            source->capital_ships.mesh->GetBounds().Origin);
        auto const capital_result{ml::make_level_simulation_init_data(*inside_capital)};
        TestRunner->TestFalse(TEXT("A fighter slot inside its capital aborts level loading"),
                              capital_result.has_value());
        if (!capital_result) {
            TestRunner->TestTrue(
                TEXT("Capital intersection has a level-start diagnostic"),
                capital_result.error().format().Contains(TEXT("intersects the capital collision")));
        }

        auto* const overlapping_fighters{
            DuplicateObject<USpaceGameLevelConfig>(source, GetTransientPackage())};
        overlapping_fighters->capital_ships.fighter_spawn_slots_relative_transforms[1].SetLocation(
            overlapping_fighters->capital_ships.fighter_spawn_slots_relative_transforms[0]
                .GetLocation());
        auto const fighter_result{ml::make_level_simulation_init_data(*overlapping_fighters)};
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
        auto const* const source{ml::load_default_level_config()};
        if (!TestRunner->TestNotNull(TEXT("Default level config loads"), source)) {
            return;
        }

        auto* const copy{DuplicateObject<USpaceGameLevelConfig>(source, GetTransientPackage())};
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

    TEST_METHOD(LegacyDefaultValuesWerePreserved)
    {
        auto const* const migrated{ml::load_default_level_config()};
        static TSoftObjectPtr<UTestSimulationConfig> const legacy_path{
            FSoftObjectPath{TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/"
                                 "DA_default_test_simulation_config."
                                 "DA_default_test_simulation_config")}};
        auto const* const legacy{legacy_path.LoadSynchronous()};
        if (!TestRunner->TestNotNull(TEXT("Migrated level config loads"), migrated) ||
            !TestRunner->TestNotNull(TEXT("Legacy test config loads"), legacy) ||
            !TestRunner->TestNotNull(TEXT("Legacy simulation bundle exists"),
                                     legacy ? legacy->simulation_config.Get() : nullptr)) {
            return;
        }

        auto const& bundle{*legacy->simulation_config};
        auto const* const player{bundle.player_ship_config.Get()};
        auto const* const projectiles{bundle.lasers_config.Get()};
        auto const* const capitals{bundle.capital_ships_config.Get()};
        auto const* const fighters{bundle.capital_ship_fighters_config.Get()};
        auto const* const turrets{bundle.static_turrets_config.Get()};
        auto const* const spinners{bundle.tube_spinners_config.Get()};
        if (!player || !projectiles || !capitals || !fighters || !turrets || !spinners) {
            TestRunner->AddError(TEXT("Legacy bundle contains a null domain config"));
            return;
        }

        TestRunner->TestTrue(TEXT("Player controller class was preserved"),
                             migrated->classes.player_controller_class ==
                                 legacy->player_controller_class);
        TestRunner->TestEqual(TEXT("Player laser damage was preserved"),
                              migrated->player_ship.laser.damage,
                              player->laser_damage);
        TestRunner->TestEqual(TEXT("Player laser cooldown was preserved"),
                              migrated->player_ship.laser.fire_cooldown,
                              player->laser_firing_period);
        TestRunner->TestTrue(TEXT("Player team visual was preserved"),
                             migrated->player_ship.team_visual_data == player->team_visual_data);
        TestRunner->TestTrue(TEXT("Projectile mesh was preserved"),
                             migrated->laser_projectiles.mesh == projectiles->mesh);
        TestRunner->TestTrue(TEXT("Projectile material was preserved"),
                             migrated->laser_projectiles.material == projectiles->material);
        TestRunner->TestEqual(TEXT("Capital health was preserved"),
                              migrated->capital_ships.max_health,
                              capitals->max_health);
        TestRunner->TestEqual(TEXT("Capital spawn slots were preserved"),
                              migrated->capital_ships.fighter_spawn_slots_relative_transforms.Num(),
                              capitals->fighter_spawn_slots_relative_transforms.Num());
        TestRunner->TestEqual(TEXT("Fighter laser speed was preserved"),
                              migrated->fighters.laser.projectile_speed,
                              fighters->laser_speed);
        TestRunner->TestTrue(TEXT("Fighter mesh was preserved"),
                             migrated->fighters.mesh == fighters->mesh);
        TestRunner->TestEqual(TEXT("Turret cooldown was preserved"),
                              migrated->turrets.laser.fire_cooldown,
                              turrets->attack_cooldown);
        TestRunner->TestTrue(TEXT("Turret team visual was preserved"),
                             migrated->turrets.team_visual_data == turrets->team_visual_data);
        TestRunner->TestEqual(TEXT("Spinner laser distance was preserved"),
                              migrated->tube_spinners.laser.max_distance,
                              spinners->laser_max_distance);
        TestRunner->TestTrue(TEXT("Spinner mesh was preserved"),
                             migrated->tube_spinners.mesh == spinners->mesh);
    }
};
