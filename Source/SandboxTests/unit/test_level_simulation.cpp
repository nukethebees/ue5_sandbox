#include <NiagaraComponent.h>
#include <NiagaraSystem.h>
#include <SandboxISMCComponent.h>
#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestNiagaraComponent.h>
#include <SpaceGame/levels/CompileLevelEvents.h>
#include <SpaceGame/levels/LevelDefinition.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/telemetry/LevelTelemetryJson.h>
#include <SpaceGamePresentation/presentation/LevelPresentation.h>
#include <SpaceGameRendering/SparkRendererComponent.h>
#include <SpaceGameSimulation/levels/LevelEventManager.h>
#include <SpaceGameSimulation/simulation/LevelSimulation.h>

#include <SandboxCore/soa_rotator_utils.h>

#include <Dom/JsonObject.h>
#include <Engine/World.h>
#include <HAL/FileManager.h>
#include <Misc/AutomationTest.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/Paths.h>
#include <Misc/ScopeExit.h>
#include <Serialization/JsonSerializer.h>

#include <type_traits>

static_assert(std::is_const_v<std::remove_reference_t<
                  decltype(std::declval<FCapitalReadView>().entities.locations.xs[0])>>);
static_assert(
    std::is_const_v<
        std::remove_reference_t<decltype(std::declval<FFighterReadView>().entities.teams[0])>>);
static_assert(
    std::is_const_v<std::remove_reference_t<decltype(std::declval<FTurretReadView>().changes[0])>>);
static_assert(std::is_const_v<std::remove_reference_t<
                  decltype(std::declval<FLaserReadView>().entities.lifetimes_remaining[0])>>);
static_assert(std::is_const_v<std::remove_pointer_t<decltype(FLevelReadView::registry)>>);

namespace {
auto make_battle() -> FLevelSimulationInitData {
    FLevelSimulationInitData data;
    data.grid_dimensions = {16, 16, 4};
    data.cell_size = {1000.f, 1000.f, 1000.f};
    data.lasers.n_preallocated_instances = 16;
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_spawns.add_defaulted(2);
    data.capital_spawns.teams = {ETestTeam::Green, ETestTeam::White};
    data.capital_spawns.healths = {100, 100};
    data.capital_spawns.initial_spawn_delays = {60.f, 60.f};
    data.capital_spawns.spawn_cooldowns = {60.f, 60.f};
    data.capital_spawns.locations.xs = {-1000.f, 1000.f};
    auto const count{ml::ioj::FEntityAABBs::num()};
    for (int32 index{}; index < count; ++index) {
        data.entity_bounds.half_extent_xs[index] = 10.f;
        data.entity_bounds.half_extent_ys[index] = 10.f;
        data.entity_bounds.half_extent_zs[index] = 10.f;
    }
    return data;
}

auto make_overlap_response_battle() -> FLevelSimulationInitData {
    auto data{make_battle()};
    data.overlap_response.damage_per_overlap_detection = 50;
    data.capital_spawns.healths = {5000, 5000};
    data.player.Emplace();
    data.player->transform.SetLocation(FVector{-1000.0, 0.0, 0.0});
    data.player->config.lateral_adjustment_speed = 1.f;
    data.player->health = {150, 150};
    data.entity_bounds.half_extent_xs[ml::ioj::FEntityAABBs::capital_ship_index] = 100.f;
    data.entity_bounds.half_extent_ys[ml::ioj::FEntityAABBs::capital_ship_index] = 100.f;
    data.entity_bounds.half_extent_zs[ml::ioj::FEntityAABBs::capital_ship_index] = 100.f;
    return data;
}

auto make_scheduled_battle() -> FLevelSimulationInitData {
    auto data{make_battle()};
    data.capital_spawns.reset();
    data.capital_target_spawn_indices.Reset();
    data.clock_settings.tick_rate = 10.0;

    ml::FLevelBuilder builder;
    builder.set_metadata(
        {.id = ml::FLevelId{FName{TEXT("scheduled-battle")}}, .title = TEXT("Scheduled Battle")});
    builder.add_team(ml::level_teams::blue);
    builder.add_team(ml::level_teams::red);
    auto const hero{builder.add_entity({
        .id = ml::FLevelEntityId{FName{TEXT("hero")}},
        .archetype = ml::level_archetypes::capital_ship,
        .team = ml::level_teams::blue,
        .position = FVector{-1000.0, 0.0, 0.0},
    })};
    auto const enemy{builder.add_entity({
        .id = ml::FLevelEntityId{FName{TEXT("enemy")}},
        .archetype = ml::level_archetypes::capital_ship,
        .team = ml::level_teams::red,
        .position = FVector{1000.0, 0.0, 0.0},
        .spawn_time_seconds = 0.21,
    })};
    builder.set_camera({
        .target_entity_ids = {hero},
        .offset_direction = FVector{-1.0, 0.0, 0.0},
        .distance = 1000.0,
    });
    builder.set_mission({
        .mode = ml::ELevelMissionMode::KillEnemies,
        .kill_count = 1,
        .hero_entity_ids = {hero},
    });
    builder.add_mission_event({.time_seconds = 0.0, .kill_target_increase = 2});
    builder.add_mission_event({
        .time_seconds = 0.21,
        .required_kill_entity_ids = {enemy},
    });
    auto const definition{builder.finish()};
    FSimulationClock clock;
    clock.initialise(data.clock_settings);
    auto compiled{ml::compile_level_events(definition, clock, data.capital_ships, data.turrets)};
    check(compiled);
    data.level_events = MoveTemp(compiled.value());
    return data;
}

void prepare_mission(FLevelSimulation& simulation) {
    auto& mission{simulation.get_mission_manager()};
    mission.set_mission_mode(ETestMissionMode::KillEnemies);
    mission.set_kill_target(1);
    mission.set_save_mission_results(false);
    mission.add_hero_entity(simulation.get_capital_ships().get_handle(0));
    mission.add_entity_required_to_kill(simulation.get_capital_ships().get_handle(1));
    simulation.finish_initialisation();
}

void kill_enemy(FLevelSimulation& simulation) {
    DirectDamageEvents events;
    events.damaged_entities.Add(simulation.get_capital_ships().get_handle(1));
    events.instigators.Add(simulation.get_capital_ships().get_handle(0));
    events.damage_amounts.Add(100);
    simulation.get_entity_registry().queue_direct_damage_events(events);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWorldlessLevelSimulationTest,
    "Sandbox.UnitTests.LevelSimulation.WorldlessBattleAndIndependentInstances",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FWorldlessLevelSimulationTest::RunTest(FString const&) -> bool {
    FLevelSimulation first{make_battle()};
    FLevelSimulation second{make_battle()};
    TestEqual(TEXT("Construction leaves external setup open"),
              first.get_state(),
              EOrchestratorState::Uninitialised);
    first.advance(1.0);
    TestEqual(TEXT("Uninitialised simulation ignores elapsed time"),
              first.get_clock().get_completed_ticks(),
              uint64{0});
    prepare_mission(first);
    prepare_mission(second);
    TestEqual(TEXT("Finishing initialization pauses the simulation"),
              first.get_state(),
              EOrchestratorState::Paused);
    TestFalse(TEXT("No run is started without metadata"),
              first.get_level_telemetry_manager().is_run_recording());
    first.pause();
    first.pause();
    TestNull(TEXT("No player is needed"), first.get_player_ship_simulation());
    TestEqual(TEXT("Both capitals are registered"),
              first.get_entity_registry().get_num_alive_active_entities(),
              2);
    first.start();
    second.start();
    auto const dt{first.get_clock().get_tick_period()};
    first.advance(dt);
    TestEqual(TEXT("Independent clock remains at zero"),
              second.get_clock().get_completed_ticks(),
              uint64{0});
    kill_enemy(first);
    first.advance(dt);
    second.advance(dt);
    TestEqual(TEXT("Damage removes only the first battle's enemy"),
              first.get_capital_ships().get_num_instances(),
              1);
    TestEqual(
        TEXT("Other battle is unaffected"), second.get_capital_ships().get_num_instances(), 2);
    auto result{first.get_mission_manager().take_result()};
    TestTrue(TEXT("Worldless mission produces a result"), result.IsSet());
    if (result.IsSet()) {
        TestEqual(TEXT("Worldless battle succeeds"), result->state, ETestMissionState::Succeeded);
        TestEqual(TEXT("Worldless battle attributes the kill"), result->kills, 1);
    }
    TestFalse(TEXT("Mission result is delivered once"),
              first.get_mission_manager().take_result().IsSet());
    first.pause();
    auto const paused_ticks{first.get_clock().get_completed_ticks()};
    first.advance(dt);
    TestEqual(TEXT("Paused battle does not advance"),
              first.get_clock().get_completed_ticks(),
              paused_ticks);
    first.start();
    first.advance(dt);
    TestEqual(TEXT("Battle resumes"), first.get_clock().get_completed_ticks(), paused_ticks + 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLevelSimulationOverlapResponseTest,
    "Sandbox.UnitTests.LevelSimulation.OverlapDamageResolvesInTheDetectionTick",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLevelSimulationOverlapResponseTest::RunTest(FString const&) -> bool {
    FLevelSimulation simulation{make_overlap_response_battle()};
    simulation.finish_initialisation();
    simulation.start();
    auto const dt{simulation.get_clock().get_tick_period()};
    auto* player{simulation.get_player_ship_simulation()};
    if (!TestNotNull(TEXT("The overlap fixture has a movable low-health entity"), player)) {
        return false;
    }

    player->set_lateral_move_input(1.f);
    auto const player_handle{player->registry_handle};
    auto const capital{simulation.get_capital_ships().get_handle(0)};
    auto const player_id{simulation.get_entity_registry().find_unique_id(player_handle)};
    auto& registry{simulation.get_entity_registry()};

    for (int32 overlap_detection{}; overlap_detection < 3; ++overlap_detection) {
        simulation.advance(dt);
        auto const events{simulation.get_spatial_query_manager()
                              .get_collision_system()
                              .get_aabb_overlap_events()};
        TestEqual(TEXT("The tick captures one unique dynamic overlap"),
                  events.entity_entity_overlaps.num(),
                  1);
        TestEqual(
            TEXT("The tick captures no static overlap"), events.entity_static_overlaps.num(), 0);

        TestEqual(TEXT("The high-health capital receives damage in the detection tick"),
                  registry.get_health(capital),
                  5000 - (overlap_detection + 1) * 50);
        if (overlap_detection < 2) {
            TestEqual(TEXT("The low-health entity receives damage in the detection tick"),
                      registry.get_health(player_handle),
                      150 - (overlap_detection + 1) * 50);
        }
    }

    TestFalse(TEXT("The low-health entity dies after three detected overlaps"),
              registry.is_valid_alive(player_handle));
    TestEqual(TEXT("The capital receives one contribution per detected tick"),
              registry.get_health(capital),
              4850);
    TestTrue(TEXT("Overlap death uses the environmental death path"),
             registry.get_unique_entities().death_reason[player_id.id] ==
                 ETestDeathReason::Unknown);
    TestEqual(TEXT("Environmental overlap death gives no combat kill"), registry.count_kills(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLevelSimulationPlanarMovementOffsetTest,
    "Sandbox.UnitTests.LevelSimulation.PlanarMovementOffsetIsTemporary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLevelSimulationPlanarMovementOffsetTest::RunTest(FString const&) -> bool {
    auto data{make_battle()};
    data.player.Emplace();
    data.player->flight_mode = ETestSpaceShipFlightMode::PlanarVelocity;

    FLevelSimulation simulation{MoveTemp(data)};
    simulation.finish_initialisation();
    simulation.start();

    auto* const player{simulation.get_player_ship_simulation()};
    if (!TestNotNull(TEXT("Planar movement fixture has a player"), player)) {
        return false;
    }

    auto const dt{simulation.get_clock().get_tick_period()};
    auto local_velocity = [player] {
        return player->transform.InverseTransformVectorNoScale(player->velocity);
    };

    player->set_lateral_move_input(1.f);
    simulation.advance(dt);
    TestTrue(TEXT("Held lateral input adds the configured local offset"),
             FMath::IsNearlyEqual(local_velocity().Y, 3000.0, 0.1));
    TestTrue(TEXT("Lateral input does not change desired planar velocity"),
             player->target_local_planar_velocity_scale.IsNearlyZero());

    player->set_lateral_move_input(0.f);
    player->set_vertical_move_input(1.f);
    simulation.advance(dt);
    TestTrue(TEXT("Held vertical input adds the configured local offset"),
             FMath::IsNearlyEqual(local_velocity().Z, 3000.0, 0.1));
    TestTrue(TEXT("Released lateral input removes its local offset"),
             FMath::IsNearlyZero(local_velocity().Y, 0.1));

    player->set_vertical_move_input(0.f);
    simulation.advance(dt);
    auto const released_velocity{local_velocity()};
    TestTrue(TEXT("Released vertical input removes its local offset"),
             FMath::IsNearlyZero(released_velocity.Z, 0.1));
    TestTrue(TEXT("Movement offsets remain temporary"),
             player->target_local_planar_velocity_scale.IsNearlyZero());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLevelSimulationReconstructionTest,
    "Sandbox.UnitTests.LevelSimulation.OptionalReconstructionClearsRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLevelSimulationReconstructionTest::RunTest(FString const&) -> bool {
    TOptional<FLevelSimulation> simulation;
    TestFalse(TEXT("Construction can be delayed"), simulation.IsSet());
    simulation.Emplace(make_battle());
    prepare_mission(*simulation);
    simulation->start();
    simulation->advance(simulation->get_clock().get_tick_period());
    kill_enemy(*simulation);
    simulation->advance(simulation->get_clock().get_tick_period());
    simulation.Reset();
    simulation.Emplace(make_battle());
    prepare_mission(*simulation);
    TestEqual(TEXT("Fresh clock starts at zero"),
              simulation->get_clock().get_completed_ticks(),
              uint64{0});
    TestEqual(TEXT("Fresh registry contains both entities"),
              simulation->get_entity_registry().get_num_alive_active_entities(),
              2);
    TestEqual(TEXT("Fresh registry has no prior history"),
              simulation->get_entity_registry().get_num_unique_ids_issued(),
              2);
    TestFalse(TEXT("No pending result survives reconstruction"),
              simulation->get_mission_manager().take_result().IsSet());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLevelSimulationScheduledEventsTest,
    "Sandbox.UnitTests.LevelSimulation.ScheduledSpawnsAndObjectivesUseSimulationTicks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLevelSimulationScheduledEventsTest::RunTest(FString const&) -> bool {
    FLevelSimulation simulation{make_scheduled_battle()};
    TestEqual(TEXT("Tick-zero entities are available before finishing initialization"),
              simulation.get_capital_ships().get_num_instances(),
              1);
    simulation.finish_initialisation();
    TestEqual(TEXT("Tick-zero increase survives authored mission configuration"),
              simulation.get_mission_manager().get_kill_target(),
              3);
    TestEqual(TEXT("Only tick-zero entities exist initially"),
              simulation.get_capital_ships().get_num_instances(),
              1);
    TestTrue(TEXT("Future objectives prevent early completion"),
             simulation.get_mission_manager().has_pending_objective_events());

    simulation.start();
    auto const dt{simulation.get_clock().get_tick_period()};
    simulation.advance(dt);
    simulation.advance(dt);
    TestEqual(TEXT("Tick-zero increase is not dispatched again"),
              simulation.get_mission_manager().get_kill_target(),
              3);
    TestEqual(TEXT("A fractional authoring time rounds up to the next tick"),
              simulation.get_capital_ships().get_num_instances(),
              1);

    simulation.advance(dt);
    TestEqual(TEXT("The delayed entity spawns on its compiled tick"),
              simulation.get_capital_ships().get_num_instances(),
              2);
    TestEqual(TEXT("The same-tick objective resolves the spawned entity handle"),
              simulation.get_mission_manager().get_entity_handles_required_to_kill().Num(),
              1);
    TestFalse(TEXT("All authored objective events have been dispatched"),
              simulation.get_mission_manager().has_pending_objective_events());
    return true;
}

/* **************************************** */
// Initialization compatibility and spatial queries
/* **************************************** */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLevelSimulationLegacyInitialisationTest,
    "Sandbox.UnitTests.LevelSimulation.LegacyTargetsAndTurretRotations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLevelSimulationLegacyInitialisationTest::RunTest(FString const&) -> bool {
    for (auto const with_player : {false, true}) {
        auto data{make_battle()};
        if (with_player) {
            data.player.Emplace();
        }
        data.capital_target_spawn_indices = {1,
                                             FLevelSimulationInitData::player_target_spawn_index};
        data.turret_spawns.add_defaulted(2);
        data.turret_spawns.teams = {ETestTeam::Green, ETestTeam::White};
        data.turret_spawns.healths = {20, 30};
        data.turret_spawns.laser_damages = {5, 7};
        data.turret_spawns.locations.ys = {-1000.f, 1000.f};
        data.turret_transforms.Emplace(FRotator{0.0, 90.0, 0.0});
        FLevelSimulation simulation{MoveTemp(data)};
        simulation.finish_initialisation();
        auto const& capitals{simulation.get_capital_ships()};
        TestTrue(TEXT("Capital spawn indices map to registered handles"),
                 capitals.get_target_handle(0) == capitals.get_handle(1));
        auto const* player{simulation.get_player_ship_simulation()};
        TestTrue(TEXT("Player target sentinel respects optional player offset"),
                 capitals.get_target_handle(1) ==
                     (player ? player->registry_handle : FRegistryEntityHandle{}));
        TestEqual(TEXT("All legacy entities are registered"),
                  simulation.get_entity_registry().get_num_alive_active_entities(),
                  with_player ? 5 : 4);
        auto const& entities{simulation.get_entity_registry().get_entity_data()};
        auto const entity_count{entities.entity_types.Num()};
        int32 turret_count{};
        for (int32 i{}; i < entity_count; ++i) {
            if (entities.entity_types[i] == ETestEntityType::Turret) {
                auto const explicit_rotation{entities.teams[i] == ETestTeam::Green};
                TestEqual(TEXT("Legacy turret health survives conversion"),
                          entities.healths[i],
                          explicit_rotation ? 20 : 30);
                TestEqual(TEXT("Turret rotation uses the transform or defaults to zero"),
                          entities.rotations.yaws[i],
                          explicit_rotation ? 90.f : 0.f);
                ++turret_count;
            }
        }
        TestEqual(TEXT("Both legacy turrets are registered"), turret_count, 2);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLevelSimulationAuthoredInitialisationTest,
    "Sandbox.UnitTests.LevelSimulation.AuthoredInitialisationSuppressesLegacyFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLevelSimulationAuthoredInitialisationTest::RunTest(FString const&) -> bool {
    for (auto const with_mission : {false, true}) {
        auto data{make_battle()};
        if (with_mission) {
            data.level_events.initialisation.mission.Emplace();
        } else {
            data.level_events.initialisation.entity_count = 1;
        }
        FLevelSimulation simulation{MoveTemp(data)};
        simulation.finish_initialisation();
        TestEqual(TEXT("Authored mission or entity count prevents legacy capital spawns"),
                  simulation.get_capital_ships().get_num_instances(),
                  0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLevelSimulationInitialQueriesTest,
    "Sandbox.UnitTests.LevelSimulation.InitialQueriesIncludeStaticAndDynamicGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLevelSimulationInitialQueriesTest::RunTest(FString const&) -> bool {
    auto data{make_battle()};
    data.static_bounds.mins.add({-10.f, 490.f, -10.f});
    data.static_bounds.maxes.add({10.f, 510.f, 10.f});
    FLevelSimulation simulation{MoveTemp(data)};
    simulation.finish_initialisation();
    auto const& queries{simulation.get_spatial_query_manager()};
    TestEqual(TEXT("Initialization rebuilds are excluded from runtime counters"),
              queries.get_runtime_telemetry().grid_rebuild_count,
              uint64{0});
    auto const dynamic_hit{queries.trace_closest({-1100.f, 0.f, 0.f}, {-900.f, 0.f, 0.f})};
    TestTrue(TEXT("Initial capital is queryable before the first tick"),
             dynamic_hit.hit && dynamic_hit.entity == simulation.get_capital_ships().get_handle(0));
    auto const static_hit{queries.trace_closest({-100.f, 500.f, 0.f}, {100.f, 500.f, 0.f})};
    TestTrue(TEXT("Initial static collision is queryable before the first tick"),
             static_hit.hit && static_hit.static_geometry_index == 0);
    simulation.start();
    simulation.advance(simulation.get_clock().get_tick_period());
    TestTrue(TEXT("Static collision survives the first dynamic rebuild"),
             queries.trace_closest({-100.f, 500.f, 0.f}, {100.f, 500.f, 0.f}).hit);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLevelSimulationSpawnQueriesTest,
    "Sandbox.UnitTests.LevelSimulation.ScheduledSpawnIsQueryableDuringDecisions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLevelSimulationSpawnQueriesTest::RunTest(FString const&) -> bool {
    auto data{make_scheduled_battle()};
    data.turrets.target_refresh_frequency = 10.f;
    // Keep the muzzle outside the turret's collision bounds so it cannot block its own query.
    data.turrets.fire_point_offset.SetLocation(FVector{20.f, 0.f, 0.f});
    auto& initial{data.level_events.initial_spawns.turret_spawns};
    initial.add_uninitialised(1);
    initial.set(0,
                data.level_events.initialisation.entity_count++,
                FVector3f{-1000.f, 1000.f, 0.f},
                FRotator3f::ZeroRotator,
                ETestTeam::Blue,
                100,
                0);
    FLevelSimulation simulation{MoveTemp(data)};
    simulation.finish_initialisation();
    simulation.start();
    auto const dt{simulation.get_clock().get_tick_period()};
    simulation.advance(dt);
    auto const first_tick_frame_memory{simulation.get_frame_memory_stats()};
    TestEqual(TEXT("Frame memory uses the configured per-simulation capacity"),
              first_tick_frame_memory.capacity_bytes,
              SIZE_T{16 * 1024 * 1024});
    TestEqual(TEXT("Frame memory is reset after the completed tick"),
              first_tick_frame_memory.current_claimed_bytes,
              SIZE_T{0});
    TestTrue(TEXT("Turret scratch claimed frame memory during the tick"),
             first_tick_frame_memory.last_frame_root_claim_count > 0);
    TestEqual(TEXT("No turret scratch allocation survives the reset"),
              first_tick_frame_memory.outstanding_allocation_count,
              uint64{0});
    simulation.advance(dt);
    TestTrue(TEXT("Turret has no enemy before the scheduled spawn"),
             simulation.get_turrets().get_target_handles()[0].is_null());
    auto const previous_rebuilds{
        simulation.get_spatial_query_manager().get_runtime_telemetry().grid_rebuild_count};
    simulation.advance(dt);
    TestEqual(TEXT("Spawn tick rebuilds during setup and at end tick"),
              simulation.get_spatial_query_manager().get_runtime_telemetry().grid_rebuild_count,
              previous_rebuilds + 2);
    TestTrue(TEXT("Decision phase acquires the enemy spawned in the same tick"),
             simulation.get_turrets().get_target_handles()[0] ==
                 simulation.get_capital_ships().get_handle(1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLevelSimulationPresentationEquivalenceTest,
    "Sandbox.UnitTests.LevelSimulation.PresentationDoesNotChangeBattleResults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLevelSimulationPresentationEquivalenceTest::RunTest(FString const&) -> bool {
    auto world{ml::get_editor_world()};
    auto* config{ml::load_default_level_config()};
    if (!TestTrue(TEXT("Editor world is available for presentation"), world.has_value()) ||
        !TestTrue(TEXT("Presentation assets load"), IsValid(config))) {
        return false;
    }
    auto* owner{world.value()->SpawnActor<AActor>()};
    if (!TestTrue(TEXT("Presentation component owner is created"), IsValid(owner))) {
        return false;
    }
    ON_SCOPE_EXIT {
        owner->Destroy();
    };
    FLevelPresentationResources resources;
    resources.lasers = NewObject<USandboxISMCComponent>(owner);
    owner->AddInstanceComponent(resources.lasers);
    resources.lasers->RegisterComponent();
    resources.sparks = NewObject<USparkRendererComponent>(owner);
    owner->AddInstanceComponent(resources.sparks);
    resources.sparks->RegisterComponent();
    for (auto** slot :
         {&resources.capital_ships, &resources.fighters, &resources.turrets, &resources.spinners}) {
        *slot = NewObject<UInstancedStaticMeshComponent>(owner);
        owner->AddInstanceComponent(*slot);
        (*slot)->RegisterComponent();
    }
    resources.config = config->get_visual_config();
    FLevelSimulation headless{make_battle()};
    FLevelSimulation visible{make_battle()};
    TestEqual(TEXT("Presentation construction preserves initial entity count"),
              visible.get_capital_ships().get_num_instances(),
              headless.get_capital_ships().get_num_instances());
    TestEqual(TEXT("Presentation construction leaves initialization open"),
              visible.get_state(),
              EOrchestratorState::Uninitialised);
    prepare_mission(headless);
    prepare_mission(visible);
    FLevelPresentation presentation{resources, visible.get_read_view(), {}};
    using Samples = ml::TimeSeriesData<FTestEntityRegistry::EntityData>;
    Samples headless_samples;
    Samples visible_samples;
    auto record{[](Samples& samples, FLevelSimulation& simulation) {
        samples.add(simulation.get_clock().get_simulation_time(),
                    simulation.get_entity_registry().get_entity_data());
    }};
    headless.on_end_tick = [&](FLevelSimulation& simulation) {
        record(headless_samples, simulation);
    };
    visible.on_end_tick = [&](FLevelSimulation& simulation) {
        record(visible_samples, simulation);
    };
    headless.start();
    visible.start();
    auto const dt{headless.get_clock().get_tick_period()};
    constexpr int32 tick_count{8};
    for (int32 tick{}; tick < tick_count; ++tick) {
        if (tick == 2) {
            kill_enemy(headless);
            kill_enemy(visible);
        }
        headless.advance(dt);
        visible.advance(dt);
        auto const prior_presentations{presentation.get_tick_count()};
        TestEqual(
            TEXT("Advancing does not present"), prior_presentations, static_cast<uint64>(tick));
        presentation.tick(dt, visible.get_read_view());
        TestEqual(TEXT("Exactly one presentation per frame"),
                  presentation.get_tick_count(),
                  prior_presentations + 1);
        TestEqual(TEXT("Presentation sees the completed simulation"),
                  presentation.get_last_completed_tick(),
                  visible.get_clock().get_completed_ticks());
    }
    TestEqual(TEXT("Both executions record every tick"), headless_samples.num(), tick_count);
    TestEqual(TEXT("Both executions have matching sample counts"),
              visible_samples.num(),
              headless_samples.num());
    for (int32 index{}; index < headless_samples.num(); ++index) {
        auto const& a{headless_samples.value_at(index)};
        auto const& b{visible_samples.value_at(index)};
        TestTrue(TEXT("Presentation preserves health and entity lifetime"),
                 a.healths == b.healths && a.alive == b.alive);
        TestTrue(TEXT("Presentation preserves locations"),
                 a.locations.xs == b.locations.xs && a.locations.ys == b.locations.ys &&
                     a.locations.zs == b.locations.zs);
        TestTrue(TEXT("Presentation preserves entity teams and types"),
                 a.teams == b.teams && a.entity_types == b.entity_types);
    }
    auto const a{headless.get_mission_manager().take_result()};
    auto const b{visible.get_mission_manager().take_result()};
    TestTrue(TEXT("Both executions complete the mission"), a.IsSet() && b.IsSet());
    if (a.IsSet() && b.IsSet()) {
        TestEqual(TEXT("Mission outcomes match"), a->state, b->state);
        TestEqual(TEXT("Mission kill totals match"), a->kills, b->kills);
        TestEqual(TEXT("Mission completion times match"), a->elapsed_seconds, b->elapsed_seconds);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLevelPresentationFrameChangesTest,
    "Sandbox.UnitTests.LevelSimulation.PresentationReconcilesCompleteFrames",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLevelPresentationFrameChangesTest::RunTest(FString const&) -> bool {
    auto world{ml::get_editor_world()};
    auto* config{ml::load_default_level_config()};
    if (!TestTrue(TEXT("Editor world is available for presentation"), world.has_value()) ||
        !TestTrue(TEXT("Presentation assets load"), IsValid(config))) {
        return false;
    }
    auto* owner{world.value()->SpawnActor<AActor>()};
    if (!TestTrue(TEXT("Presentation component owner is created"), IsValid(owner))) {
        return false;
    }
    ON_SCOPE_EXIT {
        owner->Destroy();
    };
    FLevelPresentationResources resources;
    resources.lasers = NewObject<USandboxISMCComponent>(owner);
    owner->AddInstanceComponent(resources.lasers);
    resources.lasers->RegisterComponent();
    resources.sparks = NewObject<USparkRendererComponent>(owner);
    owner->AddInstanceComponent(resources.sparks);
    resources.sparks->RegisterComponent();
    for (auto** slot :
         {&resources.capital_ships, &resources.fighters, &resources.turrets, &resources.spinners}) {
        *slot = NewObject<UInstancedStaticMeshComponent>(owner);
        owner->AddInstanceComponent(*slot);
        (*slot)->RegisterComponent();
    }
    resources.config = config->get_visual_config();

    FLevelSimulation simulation{make_scheduled_battle()};
    simulation.finish_initialisation();
    FLevelPresentation presentation{resources, simulation.get_read_view(), {}};
    simulation.start();
    auto const dt{simulation.get_clock().get_tick_period()};
    simulation.on_end_tick = [](FLevelSimulation& level) {
        if (level.get_clock().get_completed_ticks() == 3) {
            DirectDamageEvents damage;
            damage.damaged_entities.Add(level.get_capital_ships().get_handle(1));
            damage.instigators.Add(level.get_capital_ships().get_handle(0));
            damage.damage_amounts.Add(MAX_int32);
            level.get_entity_registry().queue_direct_damage_events(damage);
        }
    };
    simulation.advance(dt * 4.25);
    auto const frame{simulation.get_read_view()};
    TestEqual(TEXT("Four fixed ticks precede presentation"),
              frame.clock->get_completed_ticks(),
              uint64{4});
    TestEqual(
        TEXT("Simulation never ticks presentation"), presentation.get_tick_count(), uint64{0});
    TestEqual(TEXT("Spawn and death survive later fixed ticks"), frame.capitals.changes.Num(), 2);
    if (frame.capitals.changes.Num() == 2) {
        TestEqual(TEXT("Spawn is recorded first"),
                  frame.capitals.changes[0].kind,
                  EEntityFrameChange::Spawn);
        TestEqual(TEXT("Death follows spawn"),
                  frame.capitals.changes[1].kind,
                  EEntityFrameChange::RemoveSwap);
        TestTrue(TEXT("Changes identify the same entity"),
                 frame.capitals.changes[0].handle == frame.capitals.changes[1].handle);
    }
    TestEqual(TEXT("Death effect remains available"), frame.capitals.deaths.Num(), 1);
    presentation.tick(static_cast<float>(dt * 4.25), frame);
    TestEqual(
        TEXT("One presentation follows all fixed ticks"), presentation.get_tick_count(), uint64{1});
    TestEqual(TEXT("Presentation sees the last fixed tick"),
              presentation.get_last_completed_tick(),
              uint64{4});
    TestEqual(TEXT("Spawned and destroyed instance is reconciled"),
              resources.capital_ships->GetNumInstances(),
              1);
    TestTrue(TEXT("Fractional simulation time remains available"),
             FMath::IsNearlyEqual(frame.interpolation_alpha(), 0.25));

    simulation.advance(0.0);
    auto const idle_frame{simulation.get_read_view()};
    TestEqual(
        TEXT("Zero-step frame has no previous changes"), idle_frame.capitals.changes.Num(), 0);
    TestEqual(TEXT("Zero-step frame has no previous deaths"), idle_frame.capitals.deaths.Num(), 0);
    presentation.tick(0.f, idle_frame);
    TestEqual(
        TEXT("Zero fixed ticks still presents once"), presentation.get_tick_count(), uint64{2});
    TestEqual(
        TEXT("Zero-step frame preserves instances"), resources.capital_ships->GetNumInstances(), 1);

    FLevelPresentation attached{resources, simulation.get_read_view(), {}};
    TestEqual(TEXT("Late attachment starts from live state"),
              resources.capital_ships->GetNumInstances(),
              1);
    simulation.advance(dt);
    attached.tick(static_cast<float>(dt), simulation.get_read_view());
    TestEqual(TEXT("Late attachment does not replay old spawns"),
              resources.capital_ships->GetNumInstances(),
              1);

    auto death_data{make_battle()};
    death_data.clock_settings.tick_rate = 10.0;
    FLevelSimulation deaths{MoveTemp(death_data)};
    deaths.finish_initialisation();
    resources.config.capital_ships.n_small_explosions = 3;
    resources.config.capital_ships.small_death_explosion = NewObject<UNiagaraSystem>(owner);
    resources.config.capital_ships.main_death_explosion = NewObject<UNiagaraSystem>(owner);
    resources.config.capital_ships.time_between_explosions = 10.f;
    resources.config.capital_ships.large_explosion_delay = 100.f;
    FLevelPresentation death_effects{resources, deaths.get_read_view(), {}};
    deaths.on_end_tick = [](FLevelSimulation& level) {
        if (level.get_clock().get_completed_ticks() <= 2) {
            DirectDamageEvents damage;
            damage.damaged_entities.Add(level.get_capital_ships().get_handle(0));
            damage.instigators.Add(level.get_capital_ships().get_handle(0));
            damage.damage_amounts.Add(MAX_int32);
            level.get_entity_registry().queue_direct_damage_events(damage);
        }
    };
    deaths.start();
    deaths.advance(0.425);
    auto const death_frame{deaths.get_read_view()};
    if (!TestEqual(TEXT("Deaths from two fixed ticks survive the frame"),
                   death_frame.capitals.deaths.Num(),
                   2)) {
        return false;
    }
    TestNotEqual(TEXT("Separate fixed ticks retain separate death batches"),
                 death_frame.capitals.deaths[0].batch_index,
                 death_frame.capitals.deaths[1].batch_index);
    death_effects.tick(0.f, death_frame);
    auto const delays{death_effects.effects.get_times_remaining()};
    TestEqual(
        TEXT("Each death retains its two delayed small effects and main effect"), delays.Num(), 6);
    int32 first_small_count{};
    int32 second_small_count{};
    for (auto const delay : delays) {
        first_small_count += FMath::IsNearlyEqual(delay, 10.f) ? 1 : 0;
        second_small_count += FMath::IsNearlyEqual(delay, 20.f) ? 1 : 0;
    }
    TestEqual(TEXT("Both tick batches start their small-effect sequence independently"),
              first_small_count,
              2);
    TestEqual(
        TEXT("Both tick batches retain their second delayed small effect"), second_small_count, 2);
    death_effects.tick(0.f, death_frame);
    TestEqual(TEXT("Repeated observation cannot enqueue the same death effects twice"),
              death_effects.effects.get_times_remaining().Num(),
              6);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPlayerBoostFrameOutputTest,
    "Sandbox.UnitTests.LevelSimulation.BoostPulseSurvivesCompleteFrame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FPlayerBoostFrameOutputTest::RunTest(FString const&) -> bool {
    auto world{ml::get_editor_world()};
    auto* config{ml::load_default_level_config()};
    if (!TestTrue(TEXT("Editor world is available"), world.has_value()) ||
        !TestTrue(TEXT("Player assets load"), IsValid(config))) {
        return false;
    }
    auto* actor{ml::spawn_player_ship(
        *world.value(), config->classes.player_ship_class, &config->player_ship)};
    if (!TestTrue(TEXT("Player actor is available"), IsValid(actor))) {
        return false;
    }
    ON_SCOPE_EXIT {
        actor->Destroy();
    };
    auto data{make_battle()};
    data.clock_settings.tick_rate = 10.0;
    data.player = actor->make_spawn_data();
    data.player->config.boost_depletion_time = 0.05f;
    FLevelSimulation simulation{MoveTemp(data)};
    simulation.finish_initialisation();
    simulation.start();
    auto* player{simulation.get_player_ship_simulation()};
    auto resources{actor->get_presentation_resources()};
    auto* pulse{NewObject<UTestNiagaraComponent>(actor)};
    auto* engine{NewObject<UTestNiagaraComponent>(actor)};
    resources.pulse = pulse;
    resources.engine = engine;
    FPlayerPresentation presentation{resources, config->player_ship, player->get_read_view()};
    player->start_boost();
    simulation.advance(0.325);
    auto const frame{player->get_read_view()};
    TestEqual(TEXT("Boost has already ended after multiple fixed ticks"),
              frame.boost_brake_state,
              EBoostBrakeState::None);
    TestEqual(TEXT("Boost start remains observable without a consumer"),
              frame.boost_start_sequence,
              uint64{1});
    presentation.tick(frame);
    TestEqual(TEXT("Completed boost still triggers its pulse"), pulse->activation_count, uint64{1});
    TestFalse(TEXT("Engine effect follows final nonboosting state"), engine->active);
    pulse->Deactivate();
    presentation.tick(frame);
    TestEqual(
        TEXT("Repeated observation does not replay the pulse"), pulse->activation_count, uint64{1});
    FPlayerPresentation attached{resources, config->player_ship, frame};
    attached.tick(frame);
    TestEqual(TEXT("Late attachment does not replay historical boost starts"),
              pulse->activation_count,
              uint64{1});
    simulation.advance(0.0);
    TestEqual(TEXT("Zero-step frame preserves the boost sequence"),
              player->get_read_view().boost_start_sequence,
              uint64{1});
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLaserPresentationIndexingTest,
    "Sandbox.UnitTests.LaserPresentation.MaterialRowsTrackSimulationThroughChurn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLaserPresentationIndexingTest::RunTest(FString const&) -> bool {
    struct FExpectedMaterialData {
        FLinearColor colour;
        float initial_lifetime{};
        float spawn_time{};
    };

    auto* const component{NewObject<USandboxISMCComponent>()};
    component->set_num_custom_data_floats(FLaserPresentation::n_custom_ismc_floats);

    FLevelSimulation simulation{make_battle()};
    prepare_mission(simulation);
    auto& lasers{simulation.get_lasers()};
    FLaserPresentation presentation{*component};

    simulation.start();
    auto const dt{simulation.get_clock().get_tick_period()};
    constexpr int32 simulated_seconds{3};
    constexpr int32 spawns_per_tick{6};
    auto const tick_count{static_cast<int32>(simulation.get_clock().tick_loop.tick_rate) *
                          simulated_seconds};
    TArray<FExpectedMaterialData> expected_material_data;
    expected_material_data.Reserve(tick_count * spawns_per_tick);

    for (int32 tick{}; tick < tick_count; ++tick) {
        ml::test_lasers::SpawnRequests requests;
        requests.add_uninitialised(spawns_per_tick);
        for (int32 spawn{}; spawn < spawns_per_tick; ++spawn) {
            auto const id{expected_material_data.Num() + 1};
            auto const initial_lifetime{
                static_cast<float>((spawn == 0 ? 0.5 : 2.0 + static_cast<double>(id % 45)) * dt)};
            auto const colour{FLinearColor::White};

            ml::assign(
                requests.locations, spawn, FVector3f{0.0f, static_cast<float>(id * 10), 100000.0f});
            ml::assign(requests.rotations, spawn, FRotator3f::ZeroRotator);
            ml::assign(requests.base_velocities, spawn, FVector3f::ZeroVector);
            requests.damages[spawn] = 1;
            requests.speeds[spawn] = 1000.0f;
            requests.max_distances[spawn] = requests.speeds[spawn] * initial_lifetime;
            requests.instigator_handles[spawn] = {};
            requests.sources[spawn] = {ETestTeam::White, ETestEntityType::TubeSpinner};
            expected_material_data.Add(
                {.colour = colour,
                 .initial_lifetime = initial_lifetime,
                 .spawn_time = static_cast<float>(simulation.get_clock().get_simulation_time())});
        }

        lasers.queue_laser_spawns(requests);
        simulation.advance(dt);
        if ((tick % 3) != 2 && tick + 1 != tick_count) {
            continue;
        }
        presentation.view_ = lasers.get_read_view();
        presentation.update_visual_data();

        auto const live_count{lasers.get_num_instances()};
        if (!TestEqual(TEXT("Presentation and simulation retain the same row count"),
                       presentation.material_data.Num(),
                       live_count)) {
            return false;
        }

        for (int32 index{}; index < live_count; ++index) {
            auto const id{
                FMath::RoundToInt(lasers.get_read_view().entities.locations.ys[index] / 10.f)};
            auto const& expected{expected_material_data[id - 1]};
            auto const& actual{presentation.material_data[index]};
            auto const matches{actual.colour.X == expected.colour.R &&
                               actual.colour.Y == expected.colour.G &&
                               actual.colour.Z == expected.colour.B &&
                               actual.initial_lifetime == expected.initial_lifetime &&
                               actual.spawn_time == expected.spawn_time};
            if (!matches) {
                AddError(FString::Printf(
                    TEXT("Tick %d row %d (laser %d) has material (%.2f, %.2f, %.2f, %.6f, "
                         "%.6f), expected (%.2f, %.2f, %.2f, %.6f, %.6f)"),
                    tick,
                    index,
                    id,
                    actual.colour.X,
                    actual.colour.Y,
                    actual.colour.Z,
                    actual.initial_lifetime,
                    actual.spawn_time,
                    expected.colour.R,
                    expected.colour.G,
                    expected.colour.B,
                    expected.initial_lifetime,
                    expected.spawn_time));
                return false;
            }
        }
    }

    TestTrue(TEXT("The test exercises removal churn"),
             lasers.get_number_spawned() > lasers.get_num_instances());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLaserFrameOutputsTest,
    "Sandbox.UnitTests.LevelSimulation.ImpactsAccumulateAcrossFixedTicks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLaserFrameOutputsTest::RunTest(FString const&) -> bool {
    auto data{make_battle()};
    data.clock_settings.tick_rate = 10.0;
    FLevelSimulation simulation{MoveTemp(data)};
    prepare_mission(simulation);
    auto queue_shot = [](FLevelSimulation& level) {
        ml::test_lasers::SpawnRequests requests;
        requests.add_uninitialised(1);
        requests.locations.set(0, FVector3f{700.f, 0.f, 0.f});
        ml::assign(requests.rotations, 0, FRotator3f::ZeroRotator);
        requests.base_velocities.set(0, FVector3f::ZeroVector);
        requests.damages[0] = 1;
        requests.speeds[0] = 2000.f;
        requests.max_distances[0] = 10000.f;
        requests.instigator_handles[0] = level.get_capital_ships().get_handle(0);
        requests.sources[0] = {ETestTeam::Green, ETestEntityType::CapitalShipFighter};
        level.get_lasers().queue_laser_spawns(requests);
    };
    queue_shot(simulation);
    simulation.on_end_tick = [&](FLevelSimulation& level) {
        if (level.get_clock().get_completed_ticks() == 1) {
            queue_shot(level);
        }
    };
    simulation.start();
    simulation.advance(0.425);
    auto const frame{simulation.get_read_view()};
    TestEqual(
        TEXT("Impacted lasers leave authoritative storage"), frame.lasers.get_num_instances(), 0);
    TestEqual(TEXT("Both impacts survive the final empty fixed tick"), frame.lasers.hits.num(), 2);
    TestEqual(TEXT("Impact tick indices remain aligned"), frame.lasers.hit_ticks.Num(), 2);
    if (frame.lasers.hit_ticks.Num() == 2) {
        TestEqual(TEXT("First impact keeps its deterministic tick"),
                  frame.lasers.hit_ticks[0],
                  uint64{1});
        TestEqual(TEXT("Second impact keeps its deterministic tick"),
                  frame.lasers.hit_ticks[1],
                  uint64{2});
        TestTrue(TEXT("Neutral source is retained after removal"),
                 frame.lasers.hits.sources[0] ==
                     FLaserSource{ETestTeam::Green, ETestEntityType::CapitalShipFighter});
    }
    simulation.advance(0.0);
    TestEqual(TEXT("Next frame does not repeat consumed impacts"),
              simulation.get_read_view().lasers.hits.num(),
              0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelTelemetryRunRecordTest,
                                 "Sandbox.UnitTests.LevelTelemetryRunRecord",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

auto FLevelTelemetryRunRecordTest::RunTest(FString const&) -> bool {
    auto data{make_battle()};
    data.telemetry_metadata = FLevelTelemetryRunMetadata{
        .run_id = TEXT("12345678-1234-1234-1234-123456789abc"),
        .map_name = TEXT("TelemetryTest"),
        .level_id = TEXT("telemetry-test"),
        .level_display_name = TEXT("Telemetry Test"),
        .launched_utc = TEXT("2026-09-06T12:00:00Z"),
    };
    FLevelSimulation simulation{MoveTemp(data)};
    TestFalse(TEXT("Construction does not start telemetry recording"),
              simulation.get_level_telemetry_manager().is_run_recording());
    simulation.finish_initialisation();
    TestTrue(TEXT("Finishing starts telemetry while still paused"),
             simulation.get_level_telemetry_manager().is_run_recording() &&
                 simulation.get_state() == EOrchestratorState::Paused);
    simulation.start();

    auto& telemetry{simulation.get_level_telemetry_manager()};
    TestTrue(TEXT("Simulation initialization starts telemetry recording"),
             telemetry.is_run_recording());

    simulation.advance(1.0);
    auto const time_scale_change_tick{simulation.get_clock().get_completed_ticks() + 1};
    simulation.set_time_scale(4.0);
    simulation.advance(simulation.get_clock().get_tick_period());
    simulation.finalize_telemetry_run(ELevelTelemetryRunEndReason::WorldEnd, TEXT("test"));
    TestEqual(TEXT("Interrupted telemetry finalization does not pause simulation"),
              simulation.get_state(),
              EOrchestratorState::Running);

    auto record{telemetry.take_finalized_run()};
    if (!TestTrue(TEXT("Finalized manager yields one run record"), record.IsSet())) {
        return false;
    }
    TestFalse(TEXT("A finalized run record is yielded only once"),
              telemetry.take_finalized_run().IsSet());
    TestEqual(TEXT("Completion preserves its end reason"),
              record->completion.reason,
              ELevelTelemetryRunEndReason::WorldEnd);
    TestEqual(TEXT("Completion preserves completed ticks"),
              record->completion.completed_ticks,
              simulation.get_clock().get_completed_ticks());
    auto const& realtime{record->completed_ticks_by_real_time};
    TestTrue(TEXT("Recorder emits start, periodic, and final realtime mappings"),
             realtime.num() >= 3);
    TestEqual(TEXT("Realtime mapping starts at tick zero"), realtime.value_at(0), uint64{0});
    TestEqual(TEXT("Realtime mapping ends at the completed tick"),
              realtime.last_value(),
              simulation.get_clock().get_completed_ticks());

    auto const& series{record->tick_series};
    TestEqual(
        TEXT("An unchanged entity count is only stored once"), series.active_entities.num(), 1);
    TestEqual(TEXT("Requested time scale is only stored when it changes"),
              series.requested_time_scale.num(),
              2);
    TestEqual(TEXT("Changed requested time scale is indexed by its first simulation tick"),
              series.requested_time_scale.last_time(),
              time_scale_change_tick);
    TestEqual(TEXT("Changed requested time scale is retained"),
              series.requested_time_scale.last_value(),
              4.0);

    auto const json{serialize_level_telemetry_run(*record)};
    TSharedPtr<FJsonObject> root;
    auto reader{TJsonReaderFactory<>::Create(json)};
    if (!TestTrue(TEXT("Serialized run is valid JSON"),
                  FJsonSerializer::Deserialize(reader, root)) ||
        !TestNotNull(TEXT("Serialized run has a root object"), root.Get())) {
        return false;
    }
    TestEqual(TEXT("JSON records current schema version"),
              root->GetIntegerField(TEXT("schema_version")),
              FLevelTelemetryRunRecord::schema_version);
    auto const realtime_json{root->GetObjectField(TEXT("completed_ticks_by_real_time"))};
    TestEqual(TEXT("JSON contains realtime elapsed times"),
              realtime_json->GetArrayField(TEXT("real_elapsed_seconds")).Num(),
              realtime.num());
    TestEqual(TEXT("JSON contains corresponding completed ticks"),
              realtime_json->GetArrayField(TEXT("completed_ticks")).Num(),
              realtime.num());
    TestTrue(TEXT("JSON contains sparse workload series"),
             root->GetObjectField(TEXT("tick_series")).IsValid());
    auto const battle_samples_json{root->GetArrayField(TEXT("battle_samples"))};
    TestTrue(TEXT("JSON battle samples contain compact workload counters"),
             !battle_samples_json.IsEmpty() &&
                 battle_samples_json.Last()->AsObject()->HasField(TEXT("range_query_count")));

    root->SetStringField(TEXT("future_field"), TEXT("ignored"));
    FString json_with_unknown_field;
    auto unknown_writer{TJsonWriterFactory<>::Create(&json_with_unknown_field)};
    FJsonSerializer::Serialize(root.ToSharedRef(), unknown_writer);
    auto const round_trip{deserialize_level_telemetry_run(json_with_unknown_field)};
    if (TestTrue(TEXT("Current-schema JSON deserializes with unknown fields"),
                 round_trip.has_value())) {
        TestEqual(TEXT("Round trip preserves the run id"),
                  round_trip->metadata.run_id,
                  record->metadata.run_id);
        TestEqual(TEXT("Round trip preserves realtime mappings"),
                  round_trip->completed_ticks_by_real_time.num(),
                  realtime.num());
        TestEqual(TEXT("Round trip preserves battle workload counters"),
                  round_trip->battle_samples.Last().range_query_count,
                  record->battle_samples.Last().range_query_count);
    }
    root->SetNumberField(TEXT("schema_version"), 1);
    FString legacy_json;
    auto legacy_writer{TJsonWriterFactory<>::Create(&legacy_json)};
    FJsonSerializer::Serialize(root.ToSharedRef(), legacy_writer);
    auto const legacy_round_trip{deserialize_level_telemetry_run(legacy_json)};
    if (TestTrue(TEXT("Schema-v1 JSON remains readable"), legacy_round_trip.has_value())) {
        TestEqual(TEXT("Legacy schema is identified"), legacy_round_trip->loaded_schema_version, 1);
        TestTrue(TEXT("Legacy runs do not expose v2 battle samples"),
                 legacy_round_trip->battle_samples.IsEmpty());
    }
    root->SetNumberField(TEXT("schema_version"), FLevelTelemetryRunRecord::schema_version);
    TestFalse(TEXT("Malformed JSON returns an error"),
              deserialize_level_telemetry_run(TEXT("{")).has_value());
    auto unsupported_json{json};
    unsupported_json.ReplaceInline(TEXT("\"schema_version\": 2"), TEXT("\"schema_version\": 3"));
    TestFalse(TEXT("Unsupported schemas return an error"),
              deserialize_level_telemetry_run(unsupported_json).has_value());

    auto parse_current_root = [&root]() {
        FString mutated_json;
        auto writer{TJsonWriterFactory<>::Create(&mutated_json)};
        FJsonSerializer::Serialize(root.ToSharedRef(), writer);
        return deserialize_level_telemetry_run(mutated_json);
    };
    auto const completion_json{root->GetObjectField(TEXT("completion"))};
    auto const original_reason{completion_json->GetStringField(TEXT("reason"))};
    completion_json->SetStringField(TEXT("reason"), TEXT("future_reason"));
    TestFalse(TEXT("Unknown completion enums return an error"), parse_current_root().has_value());
    completion_json->SetStringField(TEXT("reason"), original_reason);

    auto const tick_series_json{root->GetObjectField(TEXT("tick_series"))};
    auto const active_entities_json{tick_series_json->GetObjectField(TEXT("active_entities"))};
    auto const original_active_values{active_entities_json->GetArrayField(TEXT("values"))};
    auto misaligned_active_values{original_active_values};
    misaligned_active_values.Add(MakeShared<FJsonValueNumber>(0.0));
    active_entities_json->SetArrayField(TEXT("values"), MoveTemp(misaligned_active_values));
    TestFalse(TEXT("Misaligned sparse arrays return an error"), parse_current_root().has_value());
    active_entities_json->SetArrayField(TEXT("values"), original_active_values);

    auto const requested_json{tick_series_json->GetObjectField(TEXT("requested_time_scale"))};
    auto const original_requested_ticks{requested_json->GetArrayField(TEXT("ticks"))};
    auto unordered_requested_ticks{original_requested_ticks};
    if (unordered_requested_ticks.Num() >= 2) {
        unordered_requested_ticks[1] = unordered_requested_ticks[0];
        requested_json->SetArrayField(TEXT("ticks"), MoveTemp(unordered_requested_ticks));
        TestFalse(TEXT("Non-increasing sparse ticks return an error"),
                  parse_current_root().has_value());
        requested_json->SetArrayField(TEXT("ticks"), original_requested_ticks);
    }

    auto const original_realtime_values{realtime_json->GetArrayField(TEXT("real_elapsed_seconds"))};
    auto unordered_realtime_values{original_realtime_values};
    if (unordered_realtime_values.Num() >= 2) {
        unordered_realtime_values[1] = unordered_realtime_values[0];
        realtime_json->SetArrayField(TEXT("real_elapsed_seconds"),
                                     MoveTemp(unordered_realtime_values));
        TestFalse(TEXT("Non-increasing real-time coordinates return an error"),
                  parse_current_root().has_value());
        realtime_json->SetArrayField(TEXT("real_elapsed_seconds"), original_realtime_values);
    }

    auto const spawned_json{tick_series_json->GetObjectField(TEXT("spawned_entities"))};
    auto const original_spawned_ticks{spawned_json->GetArrayField(TEXT("ticks"))};
    auto const original_spawned_values{spawned_json->GetArrayField(TEXT("values"))};
    spawned_json->SetArrayField(TEXT("ticks"), {MakeShared<FJsonValueNumber>(0.0)});
    spawned_json->SetArrayField(TEXT("values"), {MakeShared<FJsonValueNumber>(-1.0)});
    TestFalse(TEXT("Negative counter values return an error"), parse_current_root().has_value());
    spawned_json->SetArrayField(TEXT("ticks"), original_spawned_ticks);
    spawned_json->SetArrayField(TEXT("values"), original_spawned_values);

    auto const original_mission_mode{completion_json->Values.FindRef(TEXT("mission_mode"))};
    auto const original_mission_state{completion_json->Values.FindRef(TEXT("mission_state"))};
    auto const original_mission_fail{completion_json->Values.FindRef(TEXT("mission_fail_reason"))};
    completion_json->SetField(TEXT("mission_mode"), MakeShared<FJsonValueNull>());
    completion_json->SetStringField(TEXT("mission_state"), TEXT("running"));
    completion_json->SetField(TEXT("mission_fail_reason"), MakeShared<FJsonValueNull>());
    TestFalse(TEXT("Partial optional mission metadata returns an error"),
              parse_current_root().has_value());
    completion_json->SetField(TEXT("mission_mode"), original_mission_mode);
    completion_json->SetField(TEXT("mission_state"), original_mission_state);
    completion_json->SetField(TEXT("mission_fail_reason"), original_mission_fail);
    auto const original_mission_elapsed{
        completion_json->Values.FindRef(TEXT("mission_elapsed_seconds"))};
    completion_json->RemoveField(TEXT("mission_mode"));
    completion_json->RemoveField(TEXT("mission_state"));
    completion_json->RemoveField(TEXT("mission_fail_reason"));
    completion_json->RemoveField(TEXT("mission_elapsed_seconds"));
    TestTrue(TEXT("Optional mission fields may be omitted"), parse_current_root().has_value());
    completion_json->SetField(TEXT("mission_mode"), original_mission_mode);
    completion_json->SetField(TEXT("mission_state"), original_mission_state);
    completion_json->SetField(TEXT("mission_fail_reason"), original_mission_fail);
    completion_json->SetField(TEXT("mission_elapsed_seconds"), original_mission_elapsed);

    auto const output_directory{FPaths::Combine(FPaths::ProjectSavedDir(),
                                                TEXT("Automation"),
                                                TEXT("Telemetry"),
                                                FGuid::NewGuid().ToString())};
    ON_SCOPE_EXIT {
        IFileManager::Get().DeleteDirectory(*output_directory, false, true);
    };
    auto const output_path{write_level_telemetry_run(*record, output_directory)};
    if (!TestTrue(TEXT("Run writer creates an output file"), output_path.has_value())) {
        return false;
    }
    FString written_json;
    TestTrue(TEXT("Written output can be read"),
             FFileHelper::LoadFileToString(written_json, **output_path));
    TestTrue(TEXT("Atomic writer removes its temporary file"),
             IFileManager::Get().FileSize(*(*output_path + TEXT(".tmp"))) < 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelTelemetryMissionCompletionTest,
                                 "Sandbox.UnitTests.LevelTelemetryMissionCompletion",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

auto FLevelTelemetryMissionCompletionTest::RunTest(FString const&) -> bool {
    auto data{make_battle()};
    data.telemetry_metadata = FLevelTelemetryRunMetadata{
        .run_id = TEXT("12345678-1234-1234-1234-123456789abc"),
        .map_name = TEXT("TelemetryMissionTest"),
        .launched_utc = TEXT("2026-09-06T12:00:00Z"),
    };
    FLevelSimulation simulation{MoveTemp(data)};
    prepare_mission(simulation);
    simulation.start();

    auto const dt{simulation.get_clock().get_tick_period()};
    simulation.advance(dt);
    kill_enemy(simulation);
    simulation.advance(dt);

    auto const mission_result{simulation.take_mission_result()};
    TestEqual(TEXT("Taking the mission result does not pause simulation"),
              simulation.get_state(),
              EOrchestratorState::Running);
    if (!TestTrue(TEXT("Simulation yields the completed mission"), mission_result.IsSet())) {
        return false;
    }

    auto record{simulation.get_level_telemetry_manager().take_finalized_run()};
    if (!TestTrue(TEXT("Taking the mission result finalizes telemetry"), record.IsSet())) {
        return false;
    }
    TestEqual(TEXT("Mission success selects the telemetry completion reason"),
              record->completion.reason,
              ELevelTelemetryRunEndReason::MissionSucceeded);
    TestFalse(TEXT("Mission completion is not interrupted"), record->completion.interrupted);
    if (TestTrue(TEXT("Mission state is present"), record->completion.mission_state.IsSet())) {
        TestEqual(TEXT("Mission state is retained"),
                  record->completion.mission_state.GetValue(),
                  ETestMissionState::Succeeded);
    }
    return true;
}

/* **************************************** */
// Explicit telemetry completion
/* **************************************** */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLevelSimulationTelemetryCompletionTest,
    "Sandbox.UnitTests.LevelSimulation.TelemetryCompletionStopsCatchUp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLevelSimulationTelemetryCompletionTest::RunTest(FString const&) -> bool {
    auto data{make_battle()};
    data.telemetry_metadata.Emplace();
    data.telemetry_metadata->run_id = TEXT("completion-test");
    FLevelSimulation simulation{MoveTemp(data)};
    simulation.finish_initialisation();
    int32 end_tick_calls{};
    simulation.on_end_tick = [&](FLevelSimulation& level) {
        ++end_tick_calls;
        level.complete_telemetry_run(ELevelTelemetryRunEndReason::DurationReached,
                                     ETestTeam::Green);
    };
    simulation.start();
    auto const dt{simulation.get_clock().get_tick_period()};
    simulation.advance(dt * 3.25);
    TestEqual(TEXT("Completion stops catch-up after the current tick"), end_tick_calls, 1);
    TestEqual(TEXT("Completed run is paused"), simulation.get_state(), EOrchestratorState::Paused);
    TestEqual(TEXT("Only one simulation tick completes"),
              simulation.get_clock().get_completed_ticks(),
              uint64{1});
    auto& telemetry{simulation.get_level_telemetry_manager()};
    auto const record{telemetry.take_finalized_run()};
    if (TestTrue(TEXT("Completion yields a telemetry record"), record.IsSet())) {
        TestEqual(TEXT("Completion reason is retained"),
                  record->completion.reason,
                  ELevelTelemetryRunEndReason::DurationReached);
        TestFalse(TEXT("Explicit completion is not interruption"), record->completion.interrupted);
        TestTrue(TEXT("Winning team is retained"),
                 record->completion.winning_team == TOptional<ETestTeam>{ETestTeam::Green});
        TestEqual(TEXT("Completion records the current tick"),
                  record->completion.completed_ticks,
                  uint64{1});
    }
    TestFalse(TEXT("Completion is consumed once"), telemetry.take_finalized_run().IsSet());
    simulation.advance(dt * 10.0);
    TestEqual(TEXT("Completed simulation ignores paused time"),
              simulation.get_clock().get_completed_ticks(),
              uint64{1});
    simulation.on_end_tick = {};
    simulation.start();
    simulation.advance(0.0);
    TestEqual(TEXT("Completion and restart preserve accumulated simulation time"),
              simulation.get_clock().get_completed_ticks(),
              uint64{3});
    return true;
}
