#include <SandboxISMCComponent.h>
#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/test_setup.h>
#include <SpaceGame/simulation/LevelSimulation.h>

#include <SandboxCore/soa_rotator_utils.h>

#include <Engine/World.h>
#include <Misc/AutomationTest.h>
#include <Misc/ScopeExit.h>

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

void prepare_mission(FLevelSimulation& simulation) {
    auto& mission{simulation.get_mission_manager()};
    mission.set_mission_mode(ETestMissionMode::KillEnemies);
    mission.set_kill_target(1);
    mission.set_save_mission_results(false);
    mission.add_hero_entity(simulation.get_capital_ships()->get_handle(0));
    mission.add_entity_required_to_kill(simulation.get_capital_ships()->get_handle(1));
    simulation.finish_initialisation();
}

void kill_enemy(FLevelSimulation& simulation) {
    DirectDamageEvents events;
    events.damaged_entities.Add(simulation.get_capital_ships()->get_handle(1));
    events.instigators.Add(simulation.get_capital_ships()->get_handle(0));
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
    prepare_mission(first);
    prepare_mission(second);
    TestFalse(TEXT("No presentation resources are needed"), first.has_presentation());
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
              first.get_capital_ships()->get_num_instances(),
              1);
    TestEqual(
        TEXT("Other battle is unaffected"), second.get_capital_ships()->get_num_instances(), 2);
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
    for (auto** slot :
         {&resources.capital_ships, &resources.fighters, &resources.turrets, &resources.spinners}) {
        *slot = NewObject<UInstancedStaticMeshComponent>(owner);
        owner->AddInstanceComponent(*slot);
        (*slot)->RegisterComponent();
    }
    resources.config = config;
    FLevelSimulation headless{make_battle()};
    FLevelSimulation visible{make_battle(), &resources};
    prepare_mission(headless);
    prepare_mission(visible);
    TestTrue(TEXT("Components enable presentation"), visible.has_presentation());
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
        visible.commit_presentation(dt);
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
    auto* const lasers{simulation.get_lasers()};
    FLaserPresentation presentation{*component};
    presentation.bind_simulation(*lasers);

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
            auto const colour{FLinearColor{static_cast<float>(id),
                                           static_cast<float>(id) + 0.25f,
                                           static_cast<float>(id) + 0.5f}};

            ml::assign(
                requests.locations, spawn, FVector3f{static_cast<float>(id * 10), 0.0f, 100000.0f});
            ml::assign(requests.rotations, spawn, FRotator3f::ZeroRotator);
            ml::assign(requests.base_velocities, spawn, FVector3f::ZeroVector);
            requests.damages[spawn] = 1;
            requests.speeds[spawn] = 1000.0f;
            requests.max_distances[spawn] = requests.speeds[spawn] * initial_lifetime;
            requests.instigator_handles[spawn] = {};
            requests.colours[spawn] = colour;
            expected_material_data.Add(
                {.colour = colour,
                 .initial_lifetime = initial_lifetime,
                 .spawn_time = static_cast<float>(simulation.get_clock().get_simulation_time())});
        }

        lasers->queue_laser_spawns(requests);
        simulation.advance(dt);
        presentation.update_visual_data();

        auto const live_count{lasers->get_num_instances()};
        if (!TestEqual(TEXT("Presentation and simulation retain the same row count"),
                       presentation.material_data.Num(),
                       live_count)) {
            return false;
        }

        for (int32 index{}; index < live_count; ++index) {
            auto const id{FMath::RoundToInt(lasers->entities.colours[index].R)};
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
             lasers->get_number_spawned() > lasers->get_num_instances());
    return true;
}
