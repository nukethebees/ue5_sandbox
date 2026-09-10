#include <SandboxGameShared/core/SandboxDeveloperSettings.h>
#include <SpaceGameSimulation/entities/DirectDamageEvents.h>
#include <SpaceGameSimulation/simulation/LevelSimulation.h>

#include <SandboxCore/soa_vector_utils.h>

#include <CQTest.h>
#include <UObject/UObjectGlobals.h>

namespace {
auto make_cap_battle(TConstArrayView<ETestTeam> const capital_teams,
                     TConstArrayView<ETestTeam> const participating_teams,
                     int32 const max_live_fighters,
                     int32 const spawn_slots,
                     float const spawn_cooldown = 60.f) -> FLevelSimulationInitData {
    FLevelSimulationInitData data;
    data.grid_dimensions = {16, 16, 4};
    data.cell_size = {1000.f, 1000.f, 1000.f};
    data.lasers.n_preallocated_instances = 32;
    data.capital_radius = 10.f;
    data.fighter_radius = 1.f;
    data.fighters.max_live_fighters = max_live_fighters;
    for (auto const team : participating_teams) {
        data.participating_teams.add(team);
    }
    data.capital_ships.fighter_spawn_slots = spawn_slots;
    for (int32 slot_index{}; slot_index < spawn_slots; ++slot_index) {
        data.capital_ships.fighter_spawn_slots_relative_transforms.Emplace(
            FVector{100.f + slot_index * 10.f, 0.f, 0.f});
    }

    auto const capital_count{capital_teams.Num()};
    data.capital_spawns.add_defaulted(capital_count);
    data.capital_target_spawn_indices.SetNumUninitialized(capital_count);
    for (int32 capital_index{}; capital_index < capital_count; ++capital_index) {
        data.capital_spawns.locations.xs[capital_index] = capital_index * 1000.f;
        data.capital_spawns.teams[capital_index] = capital_teams[capital_index];
        data.capital_spawns.healths[capital_index] = 100;
        data.capital_spawns.initial_spawn_delays[capital_index] = 0.f;
        data.capital_spawns.spawn_cooldowns[capital_index] = spawn_cooldown;
        data.capital_target_spawn_indices[capital_index] = capital_index;
    }

    auto const bounds_count{ml::ioj::FEntityAABBs::num()};
    for (int32 index{}; index < bounds_count; ++index) {
        data.entity_bounds.half_extent_xs[index] = 10.f;
        data.entity_bounds.half_extent_ys[index] = 10.f;
        data.entity_bounds.half_extent_zs[index] = 10.f;
    }
    return data;
}

void start_and_tick(FLevelSimulation& simulation) {
    simulation.finish_initialisation();
    simulation.start();
    simulation.advance(simulation.get_clock().get_tick_period());
}

auto count_team(FLevelSimulation const& simulation, ETestTeam const team) -> int32 {
    int32 count{};
    for (auto const fighter_team : simulation.get_capital_ship_fighters().get_teams()) {
        count += fighter_team == team ? 1 : 0;
    }
    return count;
}
}

TEST_CLASS(FighterLiveCap, "Sandbox.UnitTests")
{
    TEST_METHOD(SettingsClamp)
    {
        auto* settings{NewObject<USandboxDeveloperSettings>()};
        TestRunner->TestEqual(TEXT("Default live-fighter cap"), settings->max_live_fighters, 2000);
        settings->max_live_fighters = 1;
        TestRunner->TestEqual(TEXT("Effective live-fighter cap applies hard minimum"),
                              settings->get_effective_max_live_fighters(),
                              USandboxDeveloperSettings::minimum_max_live_fighters);
    }

    TEST_METHOD(TeamPartitionsAndRemainders)
    {
        TArray<ETestTeam> all_teams{ETestTeam::White,
                                    ETestTeam::Red,
                                    ETestTeam::Green,
                                    ETestTeam::Blue,
                                    ETestTeam::Orange,
                                    ETestTeam::Yellow};
        for (auto const team_count : {1, 2, 4, 6}) {
            auto const participants{TConstArrayView<ETestTeam>{all_teams}.Left(team_count)};
            auto const remainder{team_count > 1 ? team_count - 1 : 0};
            auto data{make_cap_battle(participants, participants, team_count * 3 + remainder, 4)};
            FLevelSimulation simulation{MoveTemp(data)};
            start_and_tick(simulation);
            TestRunner->TestEqual(TEXT("Floor partition leaves the global remainder unused"),
                                  simulation.get_capital_ship_fighters().get_num_instances(),
                                  team_count * 3);
            for (auto const team : participants) {
                TestRunner->TestEqual(TEXT("Each participant receives the same partition"),
                                      count_team(simulation, team),
                                      3);
            }
        }

        TArray<ETestTeam> const one_capital{ETestTeam::White};
        TArray<ETestTeam> const two_participants{ETestTeam::White, ETestTeam::Red};
        auto data{make_cap_battle(one_capital, two_participants, 7, 8)};
        FLevelSimulation simulation{MoveTemp(data)};
        start_and_tick(simulation);
        TestRunner->TestEqual(TEXT("A team cannot borrow another participant's unused allocation"),
                              simulation.get_capital_ship_fighters().get_num_instances(),
                              3);

        TArray<ETestTeam> const repeated_capital_teams{
            ETestTeam::White, ETestTeam::White, ETestTeam::Red};
        auto inferred_data{
            make_cap_battle(repeated_capital_teams, TConstArrayView<ETestTeam>{}, 14, 8)};
        FLevelSimulation inferred_simulation{MoveTemp(inferred_data)};
        start_and_tick(inferred_simulation);
        TestRunner->TestEqual(TEXT("Legacy team inference deduplicates team sources"),
                              inferred_simulation.get_capital_ship_fighters().get_num_instances(),
                              14);

        FLevelSimulation empty_simulation{
            make_cap_battle(TConstArrayView<ETestTeam>{}, TConstArrayView<ETestTeam>{}, 10, 0)};
        empty_simulation.finish_initialisation();
        empty_simulation.start();
        empty_simulation.advance(empty_simulation.get_clock().get_tick_period());
        TestRunner->TestEqual(TEXT("A zero-team simulation remains empty"),
                              empty_simulation.get_capital_ship_fighters().get_num_instances(),
                              0);
    }

    TEST_METHOD(PartialWavesPreserveOwnership)
    {
        TArray<ETestTeam> const capitals{ETestTeam::White, ETestTeam::White};
        TArray<ETestTeam> const participants{ETestTeam::White};
        auto data{make_cap_battle(capitals, participants, 6, 4)};
        FLevelSimulation simulation{MoveTemp(data)};
        start_and_tick(simulation);
        TestRunner->TestEqual(TEXT("Full and partial waves stop at the team cap"),
                              simulation.get_capital_ship_fighters().get_num_instances(),
                              6);

        simulation.advance(simulation.get_clock().get_tick_period());
        auto const& capital_simulation{simulation.get_capital_ships()};
        TestRunner->TestEqual(TEXT("First capital owns its full accepted wave"),
                              capital_simulation.get_fighter_handles(0).Num(),
                              4);
        TestRunner->TestEqual(TEXT("Second capital owns only its accepted prefix"),
                              capital_simulation.get_fighter_handles(1).Num(),
                              2);

        auto parent_death_data{make_cap_battle(capitals, participants, 2, 1)};
        FLevelSimulation parent_death_simulation{MoveTemp(parent_death_data)};
        parent_death_simulation.finish_initialisation();
        parent_death_simulation.start();
        DirectDamageEvents capital_damage;
        capital_damage.add_uninitialised(1);
        capital_damage.damaged_entities[0] =
            parent_death_simulation.get_capital_ships().get_handle(0);
        capital_damage.instigators[0] = parent_death_simulation.get_capital_ships().get_handle(1);
        capital_damage.damage_amounts[0] = 100;
        parent_death_simulation.get_entity_registry().queue_direct_damage_events(capital_damage);
        parent_death_simulation.advance(parent_death_simulation.get_clock().get_tick_period());
        parent_death_simulation.advance(parent_death_simulation.get_clock().get_tick_period());
        TestRunner->TestEqual(
            TEXT("A surviving same-team capital adopts a new fighter from a dead parent"),
            parent_death_simulation.get_capital_ships().get_fighter_handles(0).Num(),
            2);
    }

    TEST_METHOD(DeferredRemovalAndReconstruction)
    {
        TArray<ETestTeam> const capitals{ETestTeam::White};
        auto make_data{[&] { return make_cap_battle(capitals, capitals, 1, 1, 0.f); }};
        FLevelSimulation simulation{make_data()};
        start_and_tick(simulation);
        TestRunner->TestEqual(TEXT("Initial fighter fills the budget"),
                              simulation.get_capital_ship_fighters().get_num_instances(),
                              1);

        DirectDamageEvents damage;
        damage.add_uninitialised(1);
        damage.damaged_entities[0] = simulation.get_capital_ship_fighters().get_handles()[0];
        damage.instigators[0] = simulation.get_capital_ships().get_handle(0);
        damage.damage_amounts[0] = 100000;
        simulation.get_entity_registry().queue_direct_damage_events(damage);
        simulation.advance(simulation.get_clock().get_tick_period());
        TestRunner->TestEqual(TEXT("Deferred removal finishes before capacity is reusable"),
                              simulation.get_capital_ship_fighters().get_num_instances(),
                              0);
        simulation.advance(simulation.get_clock().get_tick_period());
        TestRunner->TestEqual(TEXT("Exactly one replacement uses the released slot"),
                              simulation.get_capital_ship_fighters().get_num_instances(),
                              1);

        TOptional<FLevelSimulation> reconstructed;
        reconstructed.Emplace(make_data());
        start_and_tick(*reconstructed);
        TestRunner->TestEqual(TEXT("Reconstruction starts with a fresh budget"),
                              reconstructed->get_capital_ship_fighters().get_num_instances(),
                              1);
    }

    TEST_METHOD(UnknownTeamRejected)
    {
        TArray<ETestTeam> const capitals{ETestTeam::Green};
        TArray<ETestTeam> const participants{ETestTeam::White};
        auto data{make_cap_battle(capitals, participants, 10, 1)};
        FLevelSimulation simulation{MoveTemp(data)};
        TestRunner->AddExpectedError(
            TEXT("Rejected fighter spawn request for invalid or non-participating team"),
            EAutomationExpectedErrorFlags::Contains,
            1);
        start_and_tick(simulation);
        TestRunner->TestEqual(TEXT("Unknown team creates no fighter"),
                              simulation.get_capital_ship_fighters().get_num_instances(),
                              0);
    }
};
