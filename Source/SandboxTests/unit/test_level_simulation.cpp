#include <NiagaraComponent.h>
#include <NiagaraSystem.h>
#include <sandbox/simulation/levels/LevelEventManager.h>
#include <sandbox/simulation/simulation/LevelSimulation.h>
#include <sandbox/simulation/world_aabb_operations.h>
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
    data.capital_spawns.teams = {ml::simulation::Team::Green, ml::simulation::Team::White};
    data.capital_spawns.healths = {100, 100};
    data.capital_spawns.initial_spawn_delays = {60.f, 60.f};
    data.capital_spawns.spawn_cooldowns = {60.f, 60.f};
    data.capital_spawns.locations.xs = {-1000.f, 1000.f};
    auto const count{ml::simulation::collision::EntityAABBs::num()};
    for (int32 index{}; index < count; ++index) {
        data.entity_bounds.half_extent_xs[index] = 10.f;
        data.entity_bounds.half_extent_ys[index] = 10.f;
        data.entity_bounds.half_extent_zs[index] = 10.f;
    }
    return data;
}

auto make_scheduled_battle() -> FLevelSimulationInitData {
    auto data{make_battle()};
    data.capital_spawns.reset();
    data.capital_target_spawn_indices.clear();
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
    mission.set_mission_mode(ml::simulation::MissionMode::KillEnemies);
    mission.set_kill_target(1);
    mission.set_save_mission_results(false);
    mission.add_hero_entity(simulation.get_capital_ships().get_handle(0));
    mission.add_entity_required_to_kill(simulation.get_capital_ships().get_handle(1));
    simulation.finish_initialisation();
}

void kill_enemy(FLevelSimulation& simulation) {
    DirectDamageEvents events;
    events.add(simulation.get_capital_ships().get_handle(1),
               100,
               simulation.get_capital_ships().get_handle(0));
    simulation.get_entity_registry().queue_direct_damage_events(events);
}
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
              static_cast<int32>(
                  simulation.get_mission_manager().get_entity_handles_required_to_kill().size()),
              1);
    TestFalse(TEXT("All authored objective events have been dispatched"),
              simulation.get_mission_manager().has_pending_objective_events());
    return true;
}

/* **************************************** */
// Initialization compatibility and spatial queries
/* **************************************** */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLevelSimulationSpawnQueriesTest,
    "Sandbox.UnitTests.LevelSimulation.ScheduledSpawnIsQueryableDuringDecisions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FLevelSimulationSpawnQueriesTest::RunTest(FString const&) -> bool {
    auto data{make_scheduled_battle()};
    data.turrets.target_refresh_frequency = 10.f;
    // Keep the muzzle outside the turret's collision bounds so it cannot block its own query.
    data.turrets.fire_point_offset = ml::make_vector3f(20.f, 0.f, 0.f);
    auto& initial{data.level_events.initial_spawns.turret_spawns};
    initial.add_uninitialised(1);
    initial.entity_indices[0] = data.level_events.initialisation.entity_count++;
    initial.locations.set(0, ml::make_vector3f(-1000.f, 1000.f, 0.f));
    initial.rotations.set(0, {});
    initial.teams[0] = ml::simulation::Team::Blue;
    initial.healths[0] = 100;
    initial.laser_damages[0] = 0;
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
    resources.fighters = NewObject<USandboxISMCComponent>(owner);
    owner->AddInstanceComponent(resources.fighters);
    resources.fighters->RegisterComponent();
    for (auto** slot : {&resources.capital_ships, &resources.turrets, &resources.spinners}) {
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

    FLevelSimulation simulation{make_scheduled_battle()};
    simulation.finish_initialisation();
    FLevelPresentation presentation{resources, simulation.get_read_view(), {}};
    simulation.start();
    auto const dt{simulation.get_clock().get_tick_period()};
    simulation.on_end_tick = [](FLevelSimulation& level) {
        if (level.get_clock().get_completed_ticks() == 3) {
            DirectDamageEvents damage;
            damage.add(level.get_capital_ships().get_handle(1),
                       MAX_int32,
                       level.get_capital_ships().get_handle(0));
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
    TestEqual(TEXT("Spawn and death survive later fixed ticks"),
              static_cast<int32>(frame.capitals.changes.size()),
              2);
    if (static_cast<int32>(frame.capitals.changes.size()) == 2) {
        TestEqual(TEXT("Spawn is recorded first"),
                  frame.capitals.changes[0].kind,
                  EEntityFrameChange::Spawn);
        TestEqual(TEXT("Death follows spawn"),
                  frame.capitals.changes[1].kind,
                  EEntityFrameChange::RemoveSwap);
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
            damage.add(level.get_capital_ships().get_handle(0),
                       MAX_int32,
                       level.get_capital_ships().get_handle(0));
            level.get_entity_registry().queue_direct_damage_events(damage);
        }
    };
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
              ml::simulation::player::BoostBrakeState::None);
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
        ml::simulation::lasers::SpawnRequests requests;
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

        lasers.queue_laser_spawns(requests.get_const_view());
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
    data.telemetry_metadata = FLevelTelemetryRunMetadata{
        .run_id = "12345678-1234-1234-1234-123456789abc",
        .map_name = "TelemetryTest",
        .level_id = "telemetry-test",
        .level_display_name = "Telemetry Test",
        .launched_utc = "2026-09-06T12:00:00Z",
    };
    FLevelSimulation simulation{MoveTemp(data)};
    simulation.finish_initialisation();
    simulation.start();
    simulation.advance(1.0);
    simulation.set_time_scale(4.0);
    simulation.advance(simulation.get_clock().get_tick_period());
    simulation.finalize_telemetry_run(ml::simulation::LevelTelemetryRunEndReason::WorldEnd, "test");
    auto record{simulation.get_level_telemetry_manager().take_finalized_run()};
    if (!TestTrue(TEXT("Telemetry JSON fixture produces a run record"), record.has_value())) {
        return false;
    }
    auto const& realtime{record->completed_ticks_by_real_time};

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
                  report.metadata.run_id);
        TestEqual(TEXT("Round trip preserves realtime mappings"),
                  round_trip->completed_ticks_by_real_time.num(),
                  realtime.num());
        TestEqual(TEXT("Round trip preserves battle workload counters"),
                  round_trip->battle_samples.Last().range_query_count,
                  report.battle_samples.Last().range_query_count);
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
