#include "test_collision_uniform_grid_scenario.h"

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>

#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/entities/TestTeam.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShips.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFighters.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/CollisionSystem.h>
#include <SpaceGame/simulation/SpatialQueryManager.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxCoreEngine/actor_utils.h>

namespace ml {
namespace {
FIntVector3 const grid_dims{400, 400, 5};
FVector3f const cell_dims{5000.f, 5000.f, 20000.f};

auto count_handle(TConstArrayView<FRegistryEntityHandle> const handles,
                  FRegistryEntityHandle const expected) -> int32 {
    int32 count{};
    for (auto const handle : handles) {
        if (handle == expected) {
            ++count;
        }
    }
    return count;
}
}

FCollisionUniformGridScenario::FCollisionUniformGridScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] { spawn_fixture(); });
}

void FCollisionUniformGridScenario::on_tear_down() {
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

void FCollisionUniformGridScenario::spawn_fixture() {
    FTransform const player_transform{FRotator::ZeroRotator, FVector{-1500.f, -1500.f, 0.f}};
    auto* const player{spawn_player_ship(context_.world,
                                         context_.config.classes.player_ship_class,
                                         &context_.config.player_ship,
                                         player_transform)};
    if (!checks.is_valid(player, TEXT("Collision-grid player ship is spawned"))) {
        return;
    }
    context_.orchestrator.set_player_ship(*player);

    auto* const capital{spawn_capital_proxy(context_.world,
                                            context_.config,
                                            checks,
                                            TEXT("collision_grid_capital"),
                                            FVector{-500.f, -500.f, 0.f})};
    if (!checks.is_valid(capital, TEXT("Collision-grid capital ship is spawned"))) {
        return;
    }

    auto* const capital_config{duplicate_capital_ships_config(context_.config, *capital)};
    if (!checks.not_nullptr(capital_config, TEXT("Collision-grid capital config is created"))) {
        return;
    }
    capital_config->spawn_delay = 0.0f;
    capital->set_actor_config(capital_config);
    capital->set_team(ETestTeam::Blue);
    capital->set_target_ship(player);

    spawn_actors<ATestStaticTurretsProxy, 1>(
        context_.world, [this](ATestStaticTurretsProxy& actor, int32, ESpawnPhase const phase) {
            if (phase == ESpawnPhase::PreSpawn) {
                actor.set_actor_config(&context_.config.turrets);
                actor.set_test_name(TEXT("collision_grid_turret"));
                actor.set_team(ETestTeam::Blue);
                return;
            }
            actor.SetActorLocation(FVector{500.f, 500.f, 0.f});
        });

    spawn_actors<ATestTubeSpinnerProxy, 1>(
        context_.world, [this](ATestTubeSpinnerProxy& actor, int32, ESpawnPhase const phase) {
            if (phase == ESpawnPhase::PreSpawn) {
                actor.set_actor_config(&context_.config.tube_spinners);
                actor.set_test_name(TEXT("collision_grid_spinner"));
                return;
            }
            actor.SetActorLocation(FVector{1500.f, 1500.f, 0.f});
        });

    ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(
        this, &FCollisionUniformGridScenario::bind_proxy_entities);
}

void FCollisionUniformGridScenario::bind_proxy_entities(FProxyEntityMap const& proxies) {
    for (auto const& [actor, identifiers] : proxies) {
        auto const* const entity{Cast<ITestEntity>(actor)};
        if (entity == nullptr) {
            continue;
        }

        auto const name{entity->get_test_name()};
        if (name == TEXT("collision_grid_capital")) {
            expected_handles_[std::to_underlying(ETestEntityType::CapitalShip)] =
                identifiers.handle;
        } else if (name == TEXT("collision_grid_turret")) {
            expected_handles_[std::to_underlying(ETestEntityType::Turret)] = identifiers.handle;
        } else if (name == TEXT("collision_grid_spinner")) {
            expected_handles_[std::to_underlying(ETestEntityType::TubeSpinner)] =
                identifiers.handle;
        }
    }
}

void FCollisionUniformGridScenario::initialise_simulation() {
    initialise_test_driver();

    test_driver->orchestrator.start_simulation();

    auto const* const player{test_driver->orchestrator.get_player_ship()};
    auto const* const capitals{test_driver->orchestrator.get_capital_ships()};
    auto const* const fighters{test_driver->orchestrator.get_capital_ship_fighters()};
    if (!checks.is_valid(player, TEXT("Collision-grid player ship is available")) ||
        !checks.is_valid(capitals, TEXT("Collision-grid capital ships are available")) ||
        !checks.is_valid(fighters, TEXT("Collision-grid fighters are available"))) {
        return;
    }

    expected_handles_[std::to_underlying(ETestEntityType::PlayerShip)] =
        player->get_entity_handle();

    auto const& grid{test_driver->orchestrator.get_spatial_query_manager()
                         .get_collision_system()
                         .get_uniform_grid()};
    checks.is_true(grid.get_grid_dims() == grid_dims,
                   TEXT("Collision grid uses the production dimensions"));
    checks.is_true(grid.get_cell_dims() == cell_dims,
                   TEXT("Collision grid uses the production cell size"));

    test_driver->orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FCollisionUniformGridScenario::on_end_tick));
    test_driver->timeline.at(sample_time, [this] { sample_grid(); }).finish_at(sample_time);
}

void FCollisionUniformGridScenario::sample_grid() {
    auto& orchestrator{test_driver->orchestrator};
    auto const& registry{orchestrator.get_entity_registry()};
    auto& collision{orchestrator.get_spatial_query_manager().get_collision_system()};
    auto& grid{collision.get_uniform_grid()};
    auto const& entity_aabbs{collision.get_entity_aabbs()};

    auto const* const fighters{orchestrator.get_capital_ship_fighters()};
    checks.is_valid(fighters, TEXT("Collision-grid fighters are available at sample time"));
    checks.is_true(fighters && !fighters->get_handles().IsEmpty(),
                   TEXT("Collision-grid fighter is placed"));
    if (fighters && !fighters->get_handles().IsEmpty()) {
        expected_handles_[std::to_underlying(ETestEntityType::CapitalShipFighter)] =
            fighters->get_handles()[0];
    }

    FSample sample{};
    auto const handle_count{expected_handles_.Num()};
    sample.expected_cell_counts.Reserve(handle_count);
    sample.found_cell_counts.Reserve(handle_count);

    for (int32 i{}; i < handle_count; ++i) {
        auto const handle{expected_handles_[i]};
        auto const entity_type{static_cast<ETestEntityType>(i)};
        checks.is_true(registry.is_valid_alive(handle),
                       FString::Printf(TEXT("Expected collision-grid %s entity is alive"),
                                       LexToString(entity_type)));
        if (!registry.is_valid_alive(handle)) {
            sample.expected_cell_counts.Add(0);
            sample.found_cell_counts.Add(0);
            continue;
        }

        auto const registered_type{registry.get_entity_type(handle)};
        auto const location{registry.get_location(handle)};
        auto const half_extents{entity_aabbs.get_half_extents(std::to_underlying(registered_type))};
        auto const min_coord{grid.to_min_cell_coord(location - half_extents)};
        auto const max_coord{grid.to_max_cell_coord(location + half_extents)};

        auto const is_in_bounds{grid.is_cell_coord_in_bounds(min_coord) &&
                                grid.is_cell_coord_in_bounds(max_coord)};
        checks.is_true(is_in_bounds,
                       FString::Printf(TEXT("Expected collision-grid %s entity is placed"),
                                       LexToString(entity_type)));
        if (!is_in_bounds) {
            sample.expected_cell_counts.Add(0);
            sample.found_cell_counts.Add(0);
            continue;
        }

        int32 expected_cell_count{};
        int32 found_cell_count{};
        for (int32 x{min_coord.X}; x <= max_coord.X; ++x) {
            for (int32 y{min_coord.Y}; y <= max_coord.Y; ++y) {
                for (int32 z{min_coord.Z}; z <= max_coord.Z; ++z) {
                    ++expected_cell_count;
                    found_cell_count += count_handle(grid.get_cell_entities({x, y, z}), handle);
                }
            }
        }

        sample.expected_cell_counts.Add(expected_cell_count);
        sample.found_cell_counts.Add(found_cell_count);
    }

    samples_.add(test_driver->get_time(), MoveTemp(sample));
}

void FCollisionUniformGridScenario::on_end_tick(ATestBatchOrchestrator&) {
    test_driver->advance_timeline();
}

void FCollisionUniformGridScenario::check_results() {
    checks.is_true(!samples_.is_empty(), TEXT("Collision-grid membership sample is recorded"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const& sample{samples_.last_value()};
    auto const handle_count{expected_handles_.Num()};
    checks.are_equal(handle_count,
                     sample.expected_cell_counts.Num(),
                     TEXT("All expected entity types have a cell count"));
    checks.are_equal(handle_count,
                     sample.found_cell_counts.Num(),
                     TEXT("All expected entity types have a membership count"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    for (int32 i{}; i < handle_count; ++i) {
        checks.are_equal(sample.expected_cell_counts[i],
                         sample.found_cell_counts[i],
                         TEXT("Expected entity has the expected collision-grid membership"),
                         i);
    }
}

void FCollisionUniformGridScenario::run() {
    run_until_timeline_finished(
        [this] { initialise_simulation(); }, timeout, [this] { check_results(); });
}
}
