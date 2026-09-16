#include <ioj/sim/level_sim.h>
#include <ioj/sim/levels/level_event_manager.h>
#include <ioj/sim/testing/level_sim_test_access.h>
#include <ioj/sim/world_aabb_operations.h>
#include <NiagaraComponent.h>
#include <NiagaraSystem.h>
#include <SandboxISMCComponent.h>
#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestNiagaraComponent.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>
#include <SpaceGame/levels/CompileLevelEvents.h>
#include <SpaceGame/levels/LevelDefinition.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/telemetry/LevelTelemetryJson.h>
#include <SpaceGamePresentation/presentation/LevelPresentation.h>
#include <SpaceGameRendering/SparkRendererComponent.h>
#include <SpaceGameSimulation/entities/NativeEntityTypes.h>
#include <SpaceGameSimulation/simulation/NativeRotatorTypes.h>
#include <SpaceGameSimulation/simulation/NativeTransformTypes.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>

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
                  decltype(std::declval<::ioj::sim::CapitalReadView>().entities.locations.xs[0])>>);
static_assert(std::is_const_v<std::remove_reference_t<
                  decltype(std::declval<::ioj::sim::FighterReadView>().entities.teams[0])>>);
static_assert(
    std::is_const_v<
        std::remove_reference_t<decltype(std::declval<::ioj::sim::TurretReadView>().changes[0])>>);
static_assert(
    std::is_const_v<std::remove_reference_t<
        decltype(std::declval<::ioj::sim::LaserReadView>().entities.lifetimes_remaining[0])>>);
static_assert(
    std::is_const_v<std::remove_pointer_t<decltype(::ioj::sim::LevelReadView::registry)>>);

namespace {
auto make_battle() -> ::ioj::sim::LevelSimInitData {
    ::ioj::sim::LevelSimInitData data;
    data.grid_dimensions = {16, 16, 4};
    data.cell_size = {1000.f, 1000.f, 1000.f};
    data.lasers.n_preallocated_instances = 16;
    data.capital_ships.fighter_spawn_slots = 0;
    auto& spawn_storage{data.level_events.initial_spawns.capital_spawns};
    spawn_storage.add_defaulted(2);
    auto const spawns{spawn_storage.get_view().columns()};
    spawns.entity_indices[0] = 0;
    spawns.entity_indices[1] = 1;
    spawns.target_entity_indices[0] = -1;
    spawns.target_entity_indices[1] = -1;
    spawns.teams[0] = ::ioj::sim::Team::Green;
    spawns.teams[1] = ::ioj::sim::Team::White;
    spawns.healths[0] = 100;
    spawns.healths[1] = 100;
    spawns.initial_fighter_spawn_delays[0] = 60.f;
    spawns.initial_fighter_spawn_delays[1] = 60.f;
    spawns.fighter_spawn_cooldowns[0] = 60.f;
    spawns.fighter_spawn_cooldowns[1] = 60.f;
    spawns.locations.xs[0] = -1000.f;
    spawns.locations.xs[1] = 1000.f;
    data.level_events.initialisation.entity_count = 2;
    auto const count{::ioj::sim::collision::EntityAABBs::num()};
    for (int32 index{}; index < count; ++index) {
        data.entity_bounds.set_half_extents(index, {{10.f, 10.f, 10.f}});
    }
    return data;
}

auto make_scheduled_battle() -> ::ioj::sim::LevelSimInitData {
    auto data{make_battle()};
    data.level_events = {};
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
        .mode = ::ioj::sim::levels::LevelMissionMode::KillEnemies,
        .kill_count = 1,
        .hero_entity_ids = {hero},
    });
    builder.add_mission_event({.time_seconds = 0.0, .kill_target_increase = 2});
    builder.add_mission_event({
        .time_seconds = 0.21,
        .required_kill_entity_ids = {enemy},
    });
    auto const definition{builder.finish()};
    ::ioj::sim::SimClock clock;
    clock.initialise(data.clock_settings);
    auto compiled{ml::compile_level_events(definition, clock, data.capital_ships, data.turrets)};
    check(compiled);
    data.level_events = MoveTemp(compiled.value());
    return data;
}

void add_mission(::ioj::sim::LevelSimInitData& data) {
    auto& mission{data.level_events.initialisation.mission.emplace()};
    auto const entities{
        data.level_events.initial_spawns.capital_spawns.get_const_view().entity_indices()};
    mission.mode = ::ioj::sim::levels::LevelMissionMode::KillEnemies;
    mission.kill_count = 1;
    mission.save_results = false;
    mission.hero_entity_indices = {entities[0]};
    mission.required_kill_entity_indices = {entities[1]};
}

void kill_enemy(::ioj::sim::LevelSim& simulation) {
    ::ioj::sim::DirectDamageEvents events;
    events.add(simulation.get_entity_registry().get_current_id(
                   simulation.get_capital_ships().get_handle(1)),
               100,
               simulation.get_capital_ships().get_handle(0));
    ::ioj::sim::LevelSimTestAccess::queue_direct_damage_events(simulation, events.get_const_view());
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLevelSimScheduledEventsTest,
    "Sandbox.UnitTests.LevelSimulation.ScheduledSpawnsAndObjectivesUseSimulationTicks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLevelSimScheduledEventsTest::RunTest(FString const&) -> bool {
    ::ioj::sim::LevelSim simulation{make_scheduled_battle()};
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
              static_cast<int32>(
                  simulation.get_mission_manager().get_entity_handles_required_to_kill().size()),
              1);
    TestFalse(TEXT("All authored objective events have been dispatched"),
              simulation.get_mission_manager().has_pending_objective_events());
    return true;
}

/* **************************************** */
// Initialization and spatial queries
/* **************************************** */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLevelSimSpawnQueriesTest,
    "Sandbox.UnitTests.LevelSimulation.ScheduledSpawnQueryableInActionAndThinkingNextTick",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLevelSimSpawnQueriesTest::RunTest(FString const&) -> bool {
    auto data{make_scheduled_battle()};
    data.turrets.target_refresh_frequency = 10.f;
    // Keep the muzzle outside the turret's collision bounds so it cannot block its own query.
    data.turrets.fire_point_offset = ml::make_vector3f(20.f, 0.f, 0.f);
    auto& initial_storage{data.level_events.initial_spawns.turret_spawns};
    initial_storage.add_uninitialised(1);
    auto const initial{initial_storage.get_view().columns()};
    initial.entity_indices[0] = data.level_events.initialisation.entity_count++;
    initial.locations.set(0, ml::make_vector3f(-1000.f, 1000.f, 0.f));
    initial.rotations.set(0, {});
    initial.teams[0] = ::ioj::sim::Team::Blue;
    initial.healths[0] = 100;
    initial.laser_damages[0] = 0;
    ::ioj::sim::LevelSim simulation{MoveTemp(data)};
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
             !simulation.get_turrets().get_target_ids()[0].is_valid());
    simulation.advance(dt);
    TestTrue(TEXT("Thinking cannot acquire an entity created later in Action"),
             !simulation.get_turrets().get_target_ids()[0].is_valid());
    auto const spawned_handle{simulation.get_capital_ships().get_handle(1)};
    auto const spawn_hit{simulation.get_spatial_query_manager().trace_closest(
        ml::make_vector3f(980.f, 0.f, 0.f), ml::make_vector3f(1020.f, 0.f, 0.f))};
    TestTrue(TEXT("Action publishes the new entity to spatial queries"),
             spawn_hit.hit && spawn_hit.entity == spawned_handle);
    simulation.advance(dt);
    TestTrue(TEXT("Thinking acquires the spawned enemy on the following tick"),
             simulation.get_turrets().get_target_ids()[0] ==
                 simulation.get_entity_registry().get_current_id(
                     simulation.get_capital_ships().get_handle(1)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLevelSimPresentationEquivalenceTest,
    "Sandbox.UnitTests.LevelSimulation.PresentationDoesNotChangeBattleResults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLevelSimPresentationEquivalenceTest::RunTest(FString const&) -> bool {
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
    resources.fighters = NewObject<USandboxISMCComponent>(owner);
    owner->AddInstanceComponent(resources.fighters);
    resources.fighters->RegisterComponent();
    for (auto** slot : {&resources.capital_ships, &resources.turrets, &resources.spinners}) {
        *slot = NewObject<UInstancedStaticMeshComponent>(owner);
        owner->AddInstanceComponent(*slot);
        (*slot)->RegisterComponent();
    }
    resources.config = config->get_visual_config();
    auto headless_data{make_battle()};
    auto visible_data{make_battle()};
    add_mission(headless_data);
    add_mission(visible_data);
    ml::FWorldlessSimulationTest headless_harness{MoveTemp(headless_data)};
    ml::FWorldlessSimulationTest visible_harness{MoveTemp(visible_data)};
    auto& headless{headless_harness.get_simulation()};
    auto& visible{visible_harness.get_simulation()};
    TestEqual(TEXT("Presentation construction preserves initial entity count"),
              visible.get_capital_ships().get_num_instances(),
              headless.get_capital_ships().get_num_instances());
    TestEqual(TEXT("Presentation construction leaves initialization open"),
              visible.get_state(),
              ::ioj::sim::OrchestratorState::Uninitialised);
    headless.finish_initialisation();
    visible.finish_initialisation();
    FLevelPresentation presentation{resources, visible.get_read_view(), {}};
    using Samples = ml::TimeSeriesData<::ioj::sim::EntityRegistry::EntityData>;
    Samples headless_samples;
    Samples visible_samples;
    auto record{[](Samples& samples, ::ioj::sim::LevelSim& simulation) {
        ::ioj::sim::EntityRegistry::EntityData snapshot;
        snapshot.append_from(simulation.get_entity_registry().get_entity_data());
        samples.add(simulation.get_clock().get_simulation_time(), std::move(snapshot));
    }};
    headless_harness.on_end_tick = [&](::ioj::sim::LevelSim& simulation) {
        record(headless_samples, simulation);
    };
    visible_harness.on_end_tick = [&](::ioj::sim::LevelSim& simulation) {
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
        headless_harness.advance(dt);
        visible_harness.advance(dt);
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
        TestTrue(TEXT("Presentation preserves health and entity lifetime"), a.healths == b.healths);
        TestTrue(TEXT("Presentation preserves locations"),
                 a.locations.xs == b.locations.xs && a.locations.ys == b.locations.ys &&
                     a.locations.zs == b.locations.zs);
        TestTrue(TEXT("Presentation preserves entity teams and types"),
                 a.teams == b.teams && a.entity_types == b.entity_types);
    }
    auto const a{headless.take_mission_result()};
    auto const b{visible.take_mission_result()};
    TestTrue(TEXT("Both executions complete the mission"), a.has_value() && b.has_value());
    if (a.has_value() && b.has_value()) {
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
    resources.fighters = NewObject<USandboxISMCComponent>(owner);
    owner->AddInstanceComponent(resources.fighters);
    resources.fighters->RegisterComponent();
    for (auto** slot : {&resources.capital_ships, &resources.turrets, &resources.spinners}) {
        *slot = NewObject<UInstancedStaticMeshComponent>(owner);
        owner->AddInstanceComponent(*slot);
        (*slot)->RegisterComponent();
    }
    resources.config = config->get_visual_config();

    auto scheduled_battle{make_scheduled_battle()};
    auto const scheduled_spawns{
        scheduled_battle.level_events.schedule.capital_spawns.get_view().columns()};
    scheduled_spawns.locations.xs[0] = -1000.f;
    scheduled_spawns.healths[0] = 100;
    scheduled_battle.level_events.initial_spawns.capital_spawns.get_view().healths()[0] = 10000;
    scheduled_battle.overlap_response.damage_per_overlap_detection = 100;
    ::ioj::sim::LevelSim simulation{MoveTemp(scheduled_battle)};
    simulation.finish_initialisation();
    FLevelPresentation presentation{resources, simulation.get_read_view(), {}};
    simulation.start();
    auto const dt{simulation.get_clock().get_tick_period()};
    simulation.advance(dt * 4.25);
    auto const frame{simulation.get_read_view()};
    TestEqual(TEXT("Four fixed ticks precede presentation"),
              frame.clock->get_completed_ticks(),
              uint64{4});
    TestEqual(
        TEXT("Simulation never ticks presentation"), presentation.get_tick_count(), uint64{0});
    TestEqual(TEXT("Spawn and death survive later fixed ticks"),
              static_cast<int32>(frame.capitals.changes.size()),
              2);
    if (static_cast<int32>(frame.capitals.changes.size()) == 2) {
        TestEqual(TEXT("Spawn is recorded first"),
                  frame.capitals.changes[0].kind,
                  ::ioj::sim::EntityFrameChangeKind::Spawn);
        TestEqual(TEXT("Death follows spawn"),
                  frame.capitals.changes[1].kind,
                  ::ioj::sim::EntityFrameChangeKind::RemoveSwap);
        TestTrue(TEXT("Changes identify the same entity"),
                 frame.capitals.changes[0].handle == frame.capitals.changes[1].handle);
    }
    TestEqual(TEXT("Death effect remains available"),
              static_cast<int32>(frame.capitals.deaths.size()),
              1);
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
    TestEqual(TEXT("Zero-step frame has no previous changes"),
              static_cast<int32>(idle_frame.capitals.changes.size()),
              0);
    TestEqual(TEXT("Zero-step frame has no previous deaths"),
              static_cast<int32>(idle_frame.capitals.deaths.size()),
              0);
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
    ::ioj::sim::LevelSim deaths{MoveTemp(death_data)};
    deaths.finish_initialisation();
    resources.config.capital_ships.n_small_explosions = 3;
    resources.config.capital_ships.small_death_explosion = NewObject<UNiagaraSystem>(owner);
    resources.config.capital_ships.main_death_explosion = NewObject<UNiagaraSystem>(owner);
    resources.config.capital_ships.time_between_explosions = 10.f;
    resources.config.capital_ships.large_explosion_delay = 100.f;
    FLevelPresentation death_effects{resources, deaths.get_read_view(), {}};
    ::ioj::sim::DirectDamageEvents damage;
    damage.add(
        deaths.get_entity_registry().get_current_id(deaths.get_capital_ships().get_handle(0)),
        MAX_int32,
        deaths.get_capital_ships().get_handle(1));
    ::ioj::sim::LevelSimTestAccess::queue_direct_damage_events(deaths, damage.get_const_view());
    ::ioj::sim::lasers::SpawnRequests shot;
    shot.add({900.f, 0.f, 0.f},
             {},
             {},
             MAX_int32,
             1000.f,
             10000.f,
             {},
             {::ioj::sim::Team::Green, ::ioj::sim::EntityType::CapitalShip});
    ::ioj::sim::LevelSimTestAccess::queue_laser_spawns(deaths, shot.get_const_view());
    deaths.start();
    deaths.advance(0.425);
    auto const death_frame{deaths.get_read_view()};
    if (!TestEqual(TEXT("Deaths from two fixed ticks survive the frame"),
                   static_cast<int32>(death_frame.capitals.deaths.size()),
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
    data.level_events.initialisation.player_entity_index =
        data.level_events.initialisation.entity_count++;
    data.player = actor->make_spawn_data();
    data.player->config.boost_depletion_time = 0.05f;
    ::ioj::sim::LevelSim simulation{MoveTemp(data)};
    simulation.finish_initialisation();
    simulation.start();
    auto* player{simulation.get_player_ship_simulation()};
    auto resources{actor->get_presentation_resources()};
    auto* pulse{NewObject<UTestNiagaraComponent>(actor)};
    auto* engine{NewObject<UTestNiagaraComponent>(actor)};
    resources.pulse = pulse;
    resources.engine = engine;
    FPlayerPresentation presentation{resources, config->player_ship, player->get_read_view()};
    simulation.get_player_ship_commands()->start_boost();
    simulation.advance(0.325);
    auto const frame{player->get_read_view()};
    TestEqual(TEXT("Boost has already ended after multiple fixed ticks"),
              frame.boost_brake_state,
              ::ioj::sim::player::BoostBrakeState::None);
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

    auto data{make_battle()};
    add_mission(data);
    ::ioj::sim::LevelSim simulation{MoveTemp(data)};
    simulation.finish_initialisation();
    auto const& lasers{simulation.get_lasers()};
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
        ::ioj::sim::lasers::SpawnRequests requests;
        requests.add_uninitialised(spawns_per_tick);
        for (int32 spawn{}; spawn < spawns_per_tick; ++spawn) {
            auto const id{expected_material_data.Num() + 1};
            auto const initial_lifetime{
                static_cast<float>((spawn == 0 ? 0.5 : 2.0 + static_cast<double>(id % 45)) * dt)};
            auto const colour{FLinearColor::White};

            requests.locations.set(
                spawn, ml::to_native(FVector3f{0.0f, static_cast<float>(id * 10), 100000.0f}));
            requests.rotations.set(spawn, ml::to_native(FRotator3f::ZeroRotator));
            requests.base_velocities.set(spawn, ml::to_native(FVector3f::ZeroVector));
            requests.damages[spawn] = 1;
            requests.speeds[spawn] = 1000.0f;
            requests.max_distances[spawn] = requests.speeds[spawn] * initial_lifetime;
            requests.instigator_handles[spawn] = {};
            requests.sources[spawn] =
                ml::make_laser_source(ETestTeam::White, ETestEntityType::TubeSpinner);
            expected_material_data.Add(
                {.colour = colour,
                 .initial_lifetime = initial_lifetime,
                 .spawn_time = static_cast<float>(simulation.get_clock().get_simulation_time())});
        }

        ::ioj::sim::LevelSimTestAccess::queue_laser_spawns(simulation, requests.get_const_view());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelTelemetryRunRecordTest,
                                 "Sandbox.UnitTests.LevelTelemetryRunRecord",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

auto FLevelTelemetryRunRecordTest::RunTest(FString const&) -> bool {
    auto data{make_battle()};
    data.telemetry_metadata = ::ioj::sim::LevelTelemetryRunMetadata{
        .run_id = "12345678-1234-1234-1234-123456789abc",
        .map_name = "TelemetryTest",
        .level_id = "telemetry-test",
        .level_display_name = "Telemetry Test",
        .launched_utc = "2026-09-06T12:00:00Z",
    };
    ::ioj::sim::LevelSim simulation{MoveTemp(data)};
    simulation.finish_initialisation();
    simulation.start();
    simulation.advance(1.0);
    simulation.set_time_scale(4.0);
    simulation.advance(simulation.get_clock().get_tick_period());
    simulation.finalize_telemetry_run(::ioj::sim::LevelTelemetryRunEndReason::WorldEnd, "test");
    auto record{simulation.take_finalized_telemetry_run()};
    if (!TestTrue(TEXT("Telemetry JSON fixture produces a run record"), record.has_value())) {
        return false;
    }

    FLevelTelemetryReport report{*record};
    auto const json{serialize_level_telemetry_run(report)};
    TSharedPtr<FJsonObject> root;
    auto reader{TJsonReaderFactory<>::Create(json)};
    if (!TestTrue(TEXT("Serialized run is valid JSON"),
                  FJsonSerializer::Deserialize(reader, root)) ||
        !TestNotNull(TEXT("Serialized run has a root object"), root.Get())) {
        return false;
    }
    TestEqual(TEXT("JSON records current schema version"),
              root->GetIntegerField(TEXT("schema_version")),
              ::ioj::sim::LevelTelemetryRunRecord::schema_version);
    TestFalse(TEXT("Reports do not contain wall-clock throughput history"),
              root->HasField(TEXT("completed_ticks_by_real_time")));
    TestFalse(TEXT("Reports do not contain performance windows"),
              root->HasField(TEXT("performance_windows")));
    auto const simulation_json{root->GetObjectField(TEXT("simulation"))};
    TestFalse(TEXT("Reports do not contain timing configuration"),
              simulation_json->HasField(TEXT("detailed_timing")) ||
                  simulation_json->HasField(TEXT("detailed_timing_tick_stride")) ||
                  simulation_json->HasField(TEXT("performance_window_seconds")));
    TestFalse(TEXT("Reports do not contain measured run duration"),
              root->GetObjectField(TEXT("completion"))->HasField(TEXT("wall_elapsed_seconds")));
    auto const series_json{root->GetObjectField(TEXT("tick_series"))};
    auto const battle_samples_json{root->GetArrayField(TEXT("battle_samples"))};
    TestFalse(TEXT("Reports do not contain engineering metrics"),
              series_json->HasField(TEXT("registry_slot_count")) ||
                  series_json->HasField(TEXT("range_query_count")) ||
                  series_json->HasField(TEXT("requested_time_scale")));
    TestTrue(TEXT("Reports retain battle and projectile samples"),
             !battle_samples_json.IsEmpty() &&
                 battle_samples_json.Last()->AsObject()->HasField(TEXT("combat")) &&
                 battle_samples_json.Last()->AsObject()->HasField(TEXT("active_lasers")) &&
                 !battle_samples_json.Last()->AsObject()->HasField(TEXT("range_query_count")));

    auto parse_current_root = [&root]() {
        FString mutated_json;
        auto writer{TJsonWriterFactory<>::Create(&mutated_json)};
        FJsonSerializer::Serialize(root.ToSharedRef(), writer);
        return deserialize_level_telemetry_run(mutated_json);
    };
    root->SetStringField(TEXT("future_field"), TEXT("ignored"));
    auto const round_trip{parse_current_root()};
    if (TestTrue(TEXT("Current report round trips"), round_trip.has_value())) {
        TestEqual(TEXT("Round trip preserves the run id"),
                  round_trip->metadata.run_id,
                  report.metadata.run_id);
        TestEqual(TEXT("Round trip preserves completed ticks"),
                  round_trip->completion.completed_ticks,
                  report.completion.completed_ticks);
        TestTrue(TEXT("Round trip preserves force and combat history"),
                 round_trip->battle_samples.Num() == report.battle_samples.Num() &&
                     round_trip->battle_samples.Last().alive ==
                         report.battle_samples.Last().alive &&
                     round_trip->battle_samples.Last().combat.shots ==
                         report.battle_samples.Last().combat.shots);
    }

    root->SetStringField(TEXT("performance_windows"), TEXT("discarded malformed timing data"));
    root->SetStringField(TEXT("completed_ticks_by_real_time"), TEXT("discarded malformed mapping"));
    simulation_json->SetStringField(TEXT("detailed_timing"), TEXT("discarded"));
    simulation_json->SetStringField(TEXT("performance_window_seconds"), TEXT("discarded"));
    root->GetObjectField(TEXT("completion"))
        ->SetStringField(TEXT("wall_elapsed_seconds"), TEXT("discarded"));
    series_json->SetStringField(TEXT("range_query_count"), TEXT("discarded"));
    series_json->SetStringField(TEXT("requested_time_scale"), TEXT("discarded"));
    battle_samples_json.Last()->AsObject()->SetStringField(TEXT("range_query_count"),
                                                           TEXT("discarded"));
    for (int32 version{1}; version <= 3; ++version) {
        root->SetNumberField(TEXT("schema_version"), version);
        auto const historical{parse_current_root()};
        if (TestTrue(TEXT("Legacy reports ignore discarded performance fields"),
                     historical.has_value())) {
            TestEqual(TEXT("Legacy schema remains identified"),
                      historical->loaded_schema_version,
                      version);
            TestEqual(TEXT("Legacy reports preserve completed ticks"),
                      historical->completion.completed_ticks,
                      report.completion.completed_ticks);
            TestEqual(TEXT("Legacy reports preserve sparse force history"),
                      historical->tick_series.active_entities.num(),
                      report.tick_series.active_entities.num());
            if (version == 1) {
                TestTrue(TEXT("Version one does not fabricate combat history"),
                         historical->battle_samples.IsEmpty());
            } else {
                TestTrue(TEXT("Legacy reports preserve battle history"),
                         historical->battle_samples.Last().alive ==
                             report.battle_samples.Last().alive);
            }
            auto const upgraded{
                deserialize_level_telemetry_run(serialize_level_telemetry_run(*historical))};
            TestTrue(TEXT("Historical reports serialize using the current schema"),
                     upgraded.has_value() &&
                         upgraded->loaded_schema_version == FLevelTelemetryReport::schema_version);
        }
    }
    root->SetNumberField(TEXT("schema_version"), FLevelTelemetryReport::schema_version);
    TestFalse(TEXT("Malformed JSON returns an error"),
              deserialize_level_telemetry_run(TEXT("{")).has_value());
    root->SetNumberField(TEXT("schema_version"), 999);
    FString unsupported_json;
    auto unsupported_writer{TJsonWriterFactory<>::Create(&unsupported_json)};
    FJsonSerializer::Serialize(root.ToSharedRef(), unsupported_writer);
    TestFalse(TEXT("Unsupported schemas return an error"),
              deserialize_level_telemetry_run(unsupported_json).has_value());
    root->SetNumberField(TEXT("schema_version"),
                         ::ioj::sim::LevelTelemetryRunRecord::schema_version);

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

    auto const spawned_json{tick_series_json->GetObjectField(TEXT("spawned_entities"))};
    auto const original_spawned_ticks{spawned_json->GetArrayField(TEXT("ticks"))};
    auto const original_spawned_values{spawned_json->GetArrayField(TEXT("values"))};
    spawned_json->SetArrayField(
        TEXT("ticks"), {MakeShared<FJsonValueNumber>(0.0), MakeShared<FJsonValueNumber>(0.0)});
    spawned_json->SetArrayField(
        TEXT("values"), {MakeShared<FJsonValueNumber>(1.0), MakeShared<FJsonValueNumber>(1.0)});
    TestFalse(TEXT("Non-increasing sparse ticks return an error"),
              parse_current_root().has_value());
    spawned_json->SetArrayField(TEXT("ticks"), original_spawned_ticks);
    spawned_json->SetArrayField(TEXT("values"), original_spawned_values);

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
    auto const output_path{write_level_telemetry_run(report, output_directory)};
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
