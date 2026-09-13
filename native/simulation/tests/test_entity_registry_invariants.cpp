#include <sandbox/simulation/entities/TestEntityRegistry.h>
#include "support/simulation_test_support.h"

namespace ml::registry_tests {
auto make_handles(RegistryEntityHandles const& source) -> std::vector<FRegistryEntityHandle> {
    auto const count{source.num()};
    std::vector<FRegistryEntityHandle> handles{};
    handles.reserve(static_cast<std::size_t>(count));
    for (std::int32_t index{}; index < count; ++index) {
        handles.push_back({source.registry_indices[index], source.generations[index]});
    }
    return handles;
}

auto make_entities(std::int32_t const count, std::int32_t const offset = 0)
    -> FTestEntityRegistry::EntityData {
    FTestEntityRegistry::EntityData data;
    data.add_defaulted(count);
    for (std::int32_t index{}; index < count; ++index) {
        auto const value{static_cast<float>(offset + index + 1)};
        data.locations.set(index, ml::make_vector3f(value, value + 1.f, value + 2.f));
        data.velocities.set(index, ml::make_vector3f(value + 3.f, value + 4.f, value + 5.f));
        data.rotations.set(index, {value + 6.f, value + 7.f, value + 8.f});
        data.healths[index] = offset + index + 100;
        data.teams[index] = index % 2 == 0 ? ml::simulation::Team::Red : ml::simulation::Team::Blue;
        data.entity_types[index] = index % 2 == 0 ? ml::simulation::EntityType::Turret
                                                  : ml::simulation::EntityType::CapitalShip;
        data.alive[index] = 1;
    }
    return data;
}
}

class EntityRegistry : public ::testing::Test {
  protected:
    FTestEntityRegistry registry_;

    static auto view_of(FTestEntityRegistry::EntityData const& data,
                        std::int32_t const offset = 0,
                        std::int32_t count = -1) -> FTestEntityRegistry::EntityData::ConstView {
        if (count == -1) {
            count = data.num() - offset;
        }
        return data.get_const_view(offset, count);
    }

    void check_counts() {
        auto const& data{registry_.get_entity_data()};
        FTestEntityRegistry::EntityCounts expected{};
        std::int32_t total{};
        auto const count{data.num()};
        for (std::int32_t slot{}; slot < count; ++slot) {
            if (data.alive[slot] != 0) {
                ++expected[static_cast<std::int32_t>(data.teams[slot])]
                          [static_cast<std::int32_t>(data.entity_types[slot])];
                ++total;
            }
        }
        ml::simulation_tests::expect_equal(registry_.count_alive(), total, "Alive total");
        ml::simulation_tests::expect_equal(
            registry_.count_alive_not_on_team(ml::simulation::Team::COUNT),
            total,
            "Sentinel team excludes no entities");
        ml::simulation_tests::expect_equal(
            registry_.count_alive_not_on_team(static_cast<ml::simulation::Team>(255)),
            total,
            "Out-of-range team excludes no entities");
        ml::simulation_tests::expect_equal(
            registry_.get_num_alive_active_entities(), total, "Active alive total");
        auto const actual{registry_.count_alive_per_team_and_type()};
        auto const team_counts{registry_.count_alive_per_team()};
        constexpr auto team_count{static_cast<std::int32_t>(ml::simulation::Team::COUNT)};
        constexpr auto type_count{static_cast<std::int32_t>(ml::simulation::EntityType::COUNT)};
        for (std::int32_t team{}; team < team_count; ++team) {
            std::int32_t team_total{};
            for (std::int32_t type{}; type < type_count; ++type) {
                ml::simulation_tests::expect_equal(
                    actual[team][type], expected[team][type], "Team/type alive count");
                team_total += expected[team][type];
            }
            ml::simulation_tests::expect_equal(team_counts[team], team_total, "Team alive count");
            ml::simulation_tests::expect_equal(
                registry_.count_alive_not_on_team(static_cast<ml::simulation::Team>(team)),
                total - team_total,
                "Other teams alive count");
        }
        for (std::int32_t type{}; type < type_count; ++type) {
            std::int32_t type_total{};
            for (std::int32_t team{}; team < team_count; ++team) {
                type_total += expected[team][type];
            }
            ml::simulation_tests::expect_equal(
                registry_.count_alive(static_cast<ml::simulation::EntityType>(type)),
                type_total,
                "Type alive count");
        }
    }

    void check_row(FTestEntityRegistry::EntityData const& source,
                   std::int32_t const source_index,
                   FRegistryEntityHandle const handle) {
        auto const& actual{registry_.get_entity_data()};
        auto const slot{handle.index};
        ml::simulation_tests::expect_equal(
            actual.locations[slot], source.locations[source_index], "Location");
        ml::simulation_tests::expect_equal(
            actual.velocities[slot], source.velocities[source_index], "Velocity");
        ml::simulation_tests::expect_equal(
            actual.rotations.pitches[slot], source.rotations.pitches[source_index], "Pitch");
        ml::simulation_tests::expect_equal(
            actual.rotations.yaws[slot], source.rotations.yaws[source_index], "Yaw");
        ml::simulation_tests::expect_equal(
            actual.rotations.rolls[slot], source.rotations.rolls[source_index], "Roll");
        ml::simulation_tests::expect_equal(
            actual.healths[slot], source.healths[source_index], "Health");
        ml::simulation_tests::expect_true(actual.teams[slot] == source.teams[source_index], "Team");
        ml::simulation_tests::expect_true(
            actual.entity_types[slot] == source.entity_types[source_index], "Type");
        ml::simulation_tests::expect_equal(actual.alive[slot], source.alive[source_index], "Alive");
        auto const id{registry_.find_unique_id(handle)};
        auto const& history{registry_.get_unique_entities()};
        ml::simulation_tests::expect_equal(
            history.registry_indices[id.id], slot, "Historical slot");
        ml::simulation_tests::expect_equal(
            history.registry_generations[id.id], handle.generation, "Historical generation");
        ml::simulation_tests::expect_equal(history.life_state[id.id] ==
                                               ml::simulation::LifeState::Alive,
                                           actual.alive[slot] != 0,
                                           "Historical alive");
        ml::simulation_tests::expect_true(history.entity_types[id.id] == actual.entity_types[slot],
                                          "Historical type");
        ml::simulation_tests::expect_true(history.teams[id.id] == actual.teams[slot],
                                          "Historical team");
    }
};

TEST_F(EntityRegistry, MixedSlotReusePreservesDataAndHistoricalIdentity) {

    auto initial{ml::registry_tests::make_entities(4)};
    auto const old_handles{ml::registry_tests::make_handles(
        registry_.add_entities(view_of(initial)).registry_handles)};
    initial.alive[0] = 0;
    initial.alive[2] = 0;
    EntityDeathInfo deaths;
    deaths.add(ml::simulation::DeathReason::Combat, old_handles[0], old_handles[1]);
    deaths.add(ml::simulation::DeathReason::Unknown, old_handles[2], {});
    registry_.queue_entity_updates({old_handles, view_of(initial)}, deaths);
    registry_.commit_updates();
    check_counts();
    ml::simulation_tests::expect_true(registry_.analyse_handle(old_handles[0]) ==
                                          ml::simulation::RegistryHandleState::Active,
                                      "Dead handle still active before reuse");
    registry_.end_tick();

    auto const added{ml::registry_tests::make_entities(3, 20)};
    auto const spawned{registry_.add_entities(view_of(added))};
    auto const handles{ml::registry_tests::make_handles(spawned.registry_handles)};
    ml::simulation_tests::expect_equal(spawned.first_id.id, 4, "New IDs start after history");
    ml::simulation_tests::expect_equal(registry_.get_num_elements(), 5, "Only remainder appended");
    ml::simulation_tests::expect_true(handles[0] == FRegistryEntityHandle{2, 1},
                                      "Free slots are consumed from tail");
    ml::simulation_tests::expect_true(handles[1] == FRegistryEntityHandle{0, 1}, "Next free slot");
    ml::simulation_tests::expect_true(handles[2] == FRegistryEntityHandle{4, 0},
                                      "Appended slot starts generation zero");
    for (std::int32_t index{}; index < 3; ++index) {
        check_row(added, index, handles[index]);
        ml::simulation_tests::expect_equal(
            registry_.find_unique_id(handles[index]).id, 4 + index, "IDs follow input order");
    }
    for (auto const slot : {0, 2}) {
        ml::simulation_tests::expect_true(registry_.is_stale(old_handles[slot]),
                                          "Old handle becomes stale");
        ml::simulation_tests::expect_false(registry_.is_valid_handle(old_handles[slot]),
                                           "Old handle invalid for current data");
        ml::simulation_tests::expect_equal(registry_.find_unique_id(old_handles[slot]).id,
                                           slot,
                                           "Old handle resolves historical ID");
    }
    auto const& history{registry_.get_unique_entities()};
    ml::simulation_tests::expect_true(history.life_state[0] == ml::simulation::LifeState::Combat,
                                      "Death reason survives reuse");
    ml::simulation_tests::expect_equal(history.killed_by[0].id, 1, "Killer survives reuse");
    ml::simulation_tests::expect_false(history.killed_by[2].is_valid(),
                                       "Unattributed death has no killer");
    ml::simulation_tests::expect_true(history.life_state[0] != ml::simulation::LifeState::Alive,
                                      "Historical victim stays dead");
    check_row(initial, 1, old_handles[1]);
    check_row(initial, 3, old_handles[3]);
    check_counts();
    registry_.end_tick();
}

TEST_F(EntityRegistry, QueuedUpdateBatchesConsolidateEachEntityAndOnlyChangeMutableFields) {

    auto initial{ml::registry_tests::make_entities(3)};
    initial.alive[2] = 0;
    auto const handles{ml::registry_tests::make_handles(
        registry_.add_entities(view_of(initial)).registry_handles)};
    check_counts();
    auto first{ml::registry_tests::make_entities(2, 30)};
    first.teams[1] = ml::simulation::Team::Green;
    std::vector const first_handles{handles[0], handles[2]};
    registry_.queue_entity_updates({first_handles, view_of(first)}, {});
    auto second{ml::registry_tests::make_entities(1, 50)};
    second.teams[0] = ml::simulation::Team::Yellow;
    std::vector const second_handles{handles[1]};
    registry_.queue_entity_updates({second_handles, view_of(second)}, {});
    ml::simulation_tests::expect_equal(
        registry_.get_health(handles[0]), initial.healths[0], "Queue does not mutate live health");
    registry_.commit_updates();
    ml::simulation_tests::expect_equal(
        registry_.get_health(handles[0]), first.healths[0], "First batch updates first entity");
    ml::simulation_tests::expect_equal(
        registry_.get_health(handles[1]), second.healths[0], "Second batch updates other entity");
    ml::simulation_tests::expect_equal(
        registry_.get_health(handles[2]), first.healths[1], "Earlier batch updates third entity");
    ml::simulation_tests::expect_equal(registry_.get_location(handles[0]),
                                       first.locations[0],
                                       "Location uses consolidated update");
    ml::simulation_tests::expect_equal(
        registry_.get_velocity(handles[2]), first.velocities[1], "Velocity uses last update");
    ml::simulation_tests::expect_equal(registry_.get_entity_data().rotations.yaws[0],
                                       first.rotations.yaws[0],
                                       "Rotation uses last update");
    for (std::int32_t index{}; index < 3; ++index) {
        ml::simulation_tests::expect_true(registry_.get_entity_type(handles[index]) ==
                                              initial.entity_types[index],
                                          "Type is spawn data");
        ml::simulation_tests::expect_true(registry_.get_unique_entities().life_state[index] ==
                                              ml::simulation::LifeState::Alive,
                                          "Alive synchronized");
    }
    check_counts();
    registry_.end_tick();
    registry_.commit_updates();
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(registry_.get_dead_entities_this_frame().size()),
        0,
        "Cleanup leaves no death events");
    check_counts();
}

TEST_F(EntityRegistry, MovementReportsOnlyTransformChanges) {

    auto initial{ml::registry_tests::make_entities(4)};
    auto const handles{ml::registry_tests::make_handles(
        registry_.add_entities(view_of(initial)).registry_handles)};
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
        0,
        "Spawn is not movement");

    registry_.queue_entity_updates({handles, view_of(initial)}, {});
    registry_.commit_updates();
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
        0,
        "Identical transforms do not move");
    registry_.end_tick();
    registry_.begin_tick();

    auto updates{initial};
    updates.locations.xs[0] += 10.f;
    updates.rotations.yaws[1] += 15.f;
    updates.healths[2] -= 10;
    updates.locations.zs[3] -= 5.f;
    updates.rotations.rolls[3] += 20.f;
    registry_.queue_entity_updates({handles, view_of(updates)}, {});
    registry_.commit_updates();

    auto const moved{registry_.get_moved_entities_this_tick()};
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(moved.size()), 3, "Only transformed entities move");
    ml::simulation_tests::expect_true(moved[0] == handles[0], "Position-only turret moves");
    ml::simulation_tests::expect_true(moved[1] == handles[1], "Rotation-only capital moves");
    ml::simulation_tests::expect_true(moved[2] == handles[3], "Combined transform moves");
    ml::simulation_tests::expect_false(std::ranges::contains(moved, handles[2]),
                                       "Health-only change is not movement");

    registry_.end_tick();
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
        3,
        "Movement remains available after end tick");
    registry_.begin_tick();
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
        0,
        "Begin tick clears movement");
}

TEST_F(EntityRegistry, RepeatedLocalMovementProducesOneConsolidatedRegistryUpdate) {

    auto const initial{ml::registry_tests::make_entities(2)};
    auto const handles{ml::registry_tests::make_handles(
        registry_.add_entities(view_of(initial)).registry_handles)};

    auto first{initial};
    first.locations.xs[0] += 20.f;
    first.locations.xs[0] += 20.f;
    first.rotations.yaws[0] += 40.f;
    registry_.queue_entity_updates({std::vector{handles[0]}, view_of(first, 0, 1)}, {});
    registry_.commit_updates();

    auto moved{registry_.get_moved_entities_this_tick()};
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(moved.size()), 1, "Consolidated update reports one movement");
    ml::simulation_tests::expect_true(moved[0] == handles[0],
                                      "Consolidated movement reports the handle");

    auto second{first};
    second.locations.xs[0] += 10.f;
    registry_.queue_entity_updates({std::vector{handles[0]}, view_of(second, 0, 1)}, {});
    registry_.commit_updates();
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
        1,
        "A second commit does not duplicate movement");
    ml::simulation_tests::expect_equal(registry_.get_location(handles[0]),
                                       second.locations[0],
                                       "A second commit publishes the latest row");
    registry_.end_tick();
    registry_.begin_tick();
}

TEST_F(EntityRegistry, MovementIsPerTickAndGenerationSafeAcrossSlotReuse) {

    auto data{ml::registry_tests::make_entities(1)};
    auto const old_handle{registry_.add_entities(view_of(data)).get_handle(0)};

    data.locations.ys[0] += 10.f;
    registry_.queue_entity_updates({std::vector{old_handle}, view_of(data)}, {});
    registry_.commit_updates();
    auto moved{registry_.get_moved_entities_this_tick()};
    ml::simulation_tests::expect_true(static_cast<std::int32_t>(moved.size()) == 1 &&
                                          moved[0] == old_handle,
                                      "First tick reports movement");
    registry_.end_tick();
    registry_.begin_tick();

    data.alive[0] = 0;
    EntityDeathInfo deaths;
    deaths.add(ml::simulation::DeathReason::Unknown, old_handle, {});
    registry_.queue_entity_updates({std::vector{old_handle}, view_of(data)}, deaths);
    registry_.commit_updates();
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
        0,
        "Destruction without transform change is not movement");
    registry_.end_tick();
    registry_.begin_tick();

    auto replacement_data{ml::registry_tests::make_entities(1, 20)};
    auto const replacement{registry_.add_entities(view_of(replacement_data)).get_handle(0)};
    ml::simulation_tests::expect_true(replacement.index == old_handle.index &&
                                          replacement.generation != old_handle.generation,
                                      "Replacement reuses the slot with a new generation");
    ml::simulation_tests::expect_true(registry_.is_stale(old_handle), "Old moved handle is stale");
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
        0,
        "Replacement spawn is not stale movement");

    replacement_data.locations.zs[0] += 5.f;
    registry_.queue_entity_updates({std::vector{replacement}, view_of(replacement_data)}, {});
    registry_.commit_updates();
    moved = registry_.get_moved_entities_this_tick();
    ml::simulation_tests::expect_true(static_cast<std::int32_t>(moved.size()) == 1 &&
                                          moved[0] == replacement,
                                      "Replacement movement uses its current handle");
    registry_.end_tick();
    registry_.begin_tick();

    replacement_data.rotations.pitches[0] += 30.f;
    registry_.queue_entity_updates({std::vector{replacement}, view_of(replacement_data)}, {});
    registry_.commit_updates();
    moved = registry_.get_moved_entities_this_tick();
    ml::simulation_tests::expect_true(static_cast<std::int32_t>(moved.size()) == 1 &&
                                          moved[0] == replacement,
                                      "Successive tick reports movement again");
    registry_.end_tick();
    registry_.begin_tick();

    registry_.queue_entity_updates({std::vector{replacement}, view_of(replacement_data)}, {});
    registry_.commit_updates();
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
        0,
        "Previous movement does not leak into a quiet tick");
    registry_.end_tick();
    registry_.begin_tick();

    replacement_data.locations.xs[0] += 1.f;
    registry_.queue_entity_updates({std::vector{replacement}, view_of(replacement_data)}, {});
    registry_.commit_updates();
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
        1,
        "Movement exists before reset");
    registry_.reset();
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
        0,
        "Registry reset clears movement");
}

TEST_F(EntityRegistry, FreeSlotsBecomeReusableAtEndTickAndGenerationsAdvanceEachReuse) {

    auto data{ml::registry_tests::make_entities(1)};
    data.alive[0] = 0;
    auto const first{registry_.add_entities(view_of(data)).get_handle(0)};
    check_counts();
    EntityDeathInfo deaths;
    deaths.add(ml::simulation::DeathReason::Unknown, first, {});
    registry_.queue_entity_updates({{}, view_of(data, 0, 0)}, deaths);
    registry_.commit_updates();
    data.alive[0] = 1;
    auto const before_cleanup{registry_.add_entities(view_of(data)).get_handle(0)};
    ml::simulation_tests::expect_true(before_cleanup == FRegistryEntityHandle{1, 0},
                                      "Dead slot is not free until cleanup");
    registry_.end_tick();
    registry_.add_entities(view_of(data, 0, 0));
    auto const reused{registry_.add_entities(view_of(data)).get_handle(0)};
    ml::simulation_tests::expect_true(reused == FRegistryEntityHandle{0, 1},
                                      "Empty spawn did not consume free slot");
    data.alive[0] = 0;
    deaths.reset();
    deaths.add(ml::simulation::DeathReason::Unknown, reused, {});
    registry_.queue_entity_updates({std::vector{reused}, view_of(data)}, deaths);
    registry_.commit_updates();
    registry_.end_tick();
    data.alive[0] = 1;
    auto const next{registry_.add_entities(view_of(data)).get_handle(0)};
    ml::simulation_tests::expect_true(next == FRegistryEntityHandle{0, 2},
                                      "Second reuse increments again");
    ml::simulation_tests::expect_equal(
        registry_.find_unique_id(first).id, 0, "First occupant retains ID");
    ml::simulation_tests::expect_equal(
        registry_.find_unique_id(reused).id, 2, "Second occupant retains ID");
    ml::simulation_tests::expect_equal(
        registry_.find_unique_id(next).id, 3, "Third occupant receives new ID");
    check_counts();
    registry_.end_tick();
}

TEST_F(EntityRegistry, UniqueIdValidationRejectsNegativeAndUnissuedIds) {

    ml::simulation_tests::expect_false(registry_.is_valid_unique_id({-1}),
                                       "Negative ID rejected in empty registry");
    ml::simulation_tests::expect_false(registry_.is_valid_unique_id({0}),
                                       "Zero is initially unissued");
    auto const data{ml::registry_tests::make_entities(1)};
    registry_.add_entities(view_of(data));
    ml::simulation_tests::expect_false(registry_.is_valid_unique_id({-1}),
                                       "Negative ID rejected after spawn");
    ml::simulation_tests::expect_false(registry_.is_valid_unique_id({}), "Null ID rejected");
    ml::simulation_tests::expect_false(registry_.is_valid_unique_id({1}), "Next ID is unissued");
    ml::simulation_tests::expect_true(registry_.is_valid_unique_id({0}), "Issued ID accepted");
}

TEST_F(EntityRegistry, TeamChangesSynchronizeHistoryAndSubsequentCombatAttribution) {

    auto data{ml::registry_tests::make_entities(2)};
    auto const handles{
        ml::registry_tests::make_handles(registry_.add_entities(view_of(data)).registry_handles)};
    registry_.record_shots(std::vector{handles[0]});
    data.teams[0] = ml::simulation::Team::Green;
    data.teams[1] = ml::simulation::Team::Yellow;
    registry_.queue_entity_updates({handles, view_of(data)}, {});
    registry_.commit_updates();
    registry_.end_tick();
    check_row(data, 0, handles[0]);
    check_row(data, 1, handles[1]);
    check_counts();
    registry_.record_shots(std::vector{handles[0], FRegistryEntityHandle{}});
    DirectDamageEvents damage;
    damage.add(handles[1], 17, handles[0]);
    registry_.queue_direct_damage_events(damage);
    data.alive[1] = 0;
    EntityDeathInfo deaths;
    deaths.add(ml::simulation::DeathReason::Combat, handles[1], handles[0]);
    registry_.queue_entity_updates({handles, view_of(data)}, deaths);
    registry_.commit_updates();
    auto const& counters{registry_.get_combat_telemetry()};
    auto const red{static_cast<std::int32_t>(ml::simulation::Team::Red)};
    auto const green{static_cast<std::int32_t>(ml::simulation::Team::Green)};
    auto const yellow{static_cast<std::int32_t>(ml::simulation::Team::Yellow)};
    auto const turret{static_cast<std::int32_t>(ml::simulation::EntityType::Turret)};
    auto const capital{static_cast<std::int32_t>(ml::simulation::EntityType::CapitalShip)};
    ml::simulation_tests::expect_equal(
        counters.shots[red][turret], std::uint64_t{1}, "Earlier shot retains original team");
    ml::simulation_tests::expect_equal(
        counters.shots[green][turret], std::uint64_t{1}, "Later shot uses committed team");
    ml::simulation_tests::expect_equal(
        counters.hits[green][turret], std::uint64_t{1}, "Hit uses committed team");
    ml::simulation_tests::expect_equal(
        counters.damage_dealt[green][turret], 17.0, "Damage dealt uses committed team");
    ml::simulation_tests::expect_equal(
        counters.damage_received[yellow][capital], 17.0, "Damage received uses committed team");
    ml::simulation_tests::expect_equal(
        counters.kills[green][turret], std::uint64_t{1}, "Kill uses committed team");
    ml::simulation_tests::expect_equal(
        counters.losses[yellow][capital], std::uint64_t{1}, "Loss uses committed team");
    ml::simulation_tests::expect_equal(
        counters.destroyed[yellow][capital], std::uint64_t{1}, "Destruction uses committed team");
    ml::simulation_tests::expect_equal(
        counters.kill_matrix[green][yellow], std::uint64_t{1}, "Kill matrix uses committed teams");
    registry_.end_tick();
    auto const replacement{ml::registry_tests::make_entities(1)};
    registry_.add_entities(view_of(replacement));
    ml::simulation_tests::expect_true(registry_.get_unique_entities().teams[1] ==
                                          ml::simulation::Team::Yellow,
                                      "Reused slot preserves victim's last team");
}

TEST_F(EntityRegistry, StaleKillersRetainCreditAcrossTicksAndSlotReuse) {

    auto data{ml::registry_tests::make_entities(3)};
    auto const handles{
        ml::registry_tests::make_handles(registry_.add_entities(view_of(data)).registry_handles)};
    data.alive[0] = 0;
    data.alive[1] = 0;
    EntityDeathInfo deaths;
    deaths.add(ml::simulation::DeathReason::Combat, handles[1], handles[0]);
    deaths.add(ml::simulation::DeathReason::Unknown, handles[0], {});
    registry_.queue_entity_updates({handles, view_of(data)}, deaths);
    registry_.commit_updates();
    auto const dead{registry_.get_dead_entities_this_frame()};
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(dead.size()), 2, "Both deaths recorded");
    ml::simulation_tests::expect_true(dead[0] == handles[1] && dead[1] == handles[0],
                                      "Death order follows queue");
    ml::simulation_tests::expect_equal(
        registry_.count_kills(), 1, "Only attributed death counts as kill");
    registry_.end_tick();
    auto const replacement{ml::registry_tests::make_entities(2, 40)};
    auto const replacements{ml::registry_tests::make_handles(
        registry_.add_entities(view_of(replacement)).registry_handles)};
    ml::simulation_tests::expect_true(registry_.is_stale(handles[0]), "Killer handle is stale");
    auto update{ml::registry_tests::make_entities(1)};
    update.alive[0] = 0;
    deaths.reset();
    deaths.add(ml::simulation::DeathReason::Combat, handles[2], handles[0]);
    registry_.queue_entity_updates({std::vector{handles[2]}, view_of(update)}, deaths);
    registry_.commit_updates();
    ml::simulation_tests::expect_equal(
        registry_.get_kills({0}), std::uint32_t{2}, "Historical killer gains credit");
    ml::simulation_tests::expect_equal(
        registry_.get_unique_entities().killed_by[2].id, 0, "Victim records historical killer");
    ml::simulation_tests::expect_equal(
        registry_.get_kills(registry_.find_unique_id(replacements[1])),
        std::uint32_t{0},
        "Replacement gains no credit");
    ml::simulation_tests::expect_equal(registry_.count_kills(), 2, "Kills accumulate across ticks");
    check_counts();
    registry_.end_tick();
    registry_.commit_updates();
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(registry_.get_dead_entities_this_frame().size()),
        0,
        "Deaths cleared at end tick");
    ml::simulation_tests::expect_equal(
        registry_.count_kills(), 2, "Cleared deaths are not replayed");
}

TEST_F(EntityRegistry, RefreshDistinguishesNullStaleDeadAndLiveHandles) {

    auto data{ml::registry_tests::make_entities(3)};
    auto const handles{
        ml::registry_tests::make_handles(registry_.add_entities(view_of(data)).registry_handles)};
    ml::simulation_tests::expect_true(registry_.analyse_handle({}) ==
                                          ml::simulation::RegistryHandleState::Null,
                                      "Default is null");
    ml::simulation_tests::expect_true(registry_.analyse_handle({3, 0}) ==
                                          ml::simulation::RegistryHandleState::Invalid,
                                      "Out of range is invalid");
    ml::simulation_tests::expect_true(registry_.analyse_handle({0, 1}) ==
                                          ml::simulation::RegistryHandleState::Invalid,
                                      "Future generation is invalid");
    data.alive[0] = 0;
    EntityDeathInfo deaths;
    deaths.add(ml::simulation::DeathReason::Unknown, handles[0], {});
    registry_.queue_entity_updates({handles, view_of(data)}, deaths);
    registry_.commit_updates();
    registry_.end_tick();
    registry_.add_entities(view_of(data, 1, 1));
    data.alive[1] = 0;
    deaths.reset();
    deaths.add(ml::simulation::DeathReason::Unknown, handles[1], {});
    registry_.queue_entity_updates({std::vector{handles[1]}, view_of(data, 1, 1)}, deaths);
    registry_.commit_updates();
    std::vector refreshed{FRegistryEntityHandle{}, handles[0], handles[1], handles[2]};
    ml::simulation::Vectors3f locations;
    ml::simulation::Vectors3f velocities;
    locations.add_defaulted(4);
    velocities.add_defaulted(4);
    registry_.refresh_entity_data(refreshed, locations.get_view(), velocities.get_view());
    for (std::int32_t index{}; index < 3; ++index) {
        ml::simulation_tests::expect_true(refreshed[index].is_null(),
                                          "Null, stale and dead become null");
        ml::simulation_tests::expect_equal(
            locations[index], ml::make_vector3f(0.f, 0.f, 0.f), "Cleared location");
        ml::simulation_tests::expect_equal(
            velocities[index], ml::make_vector3f(0.f, 0.f, 0.f), "Cleared velocity");
    }
    ml::simulation_tests::expect_true(refreshed[3] == handles[2], "Live handle retained");
    ml::simulation_tests::expect_equal(locations[3], data.locations[2], "Live location refreshed");
    ml::simulation_tests::expect_equal(
        velocities[3], data.velocities[2], "Live velocity refreshed");
    registry_.refresh_entity_data(refreshed, {}, {});
    registry_.refresh_entity_data(refreshed, {}, velocities.get_view());
    registry_.refresh_entity_data(refreshed, locations.get_view(), {});
    registry_.refresh_entity_data({}, {}, {});
    std::vector const location_handles{FRegistryEntityHandle{}, handles[1]};
    ml::simulation::Vectors3f dead_locations;
    dead_locations.add_defaulted(2);
    registry_.refresh_locations(location_handles, dead_locations.get_view());
    ml::simulation_tests::expect_equal(
        dead_locations[1], data.locations[1], "Location-only refresh accepts current dead handle");
    registry_.end_tick();
}

TEST_F(EntityRegistry, DamageQueuesPreserveBatchesAndResetStartsANewIdentityLifetime) {

    auto const data{ml::registry_tests::make_entities(2)};
    auto const handles{
        ml::registry_tests::make_handles(registry_.add_entities(view_of(data)).registry_handles)};
    DirectDamageEvents first;
    first.add(handles[1], 5, handles[0]);
    first.add(handles[0], 8, {});
    DirectDamageEvents second;
    second.add(handles[1], 13, handles[0]);
    registry_.queue_direct_damage_events(first);
    registry_.queue_direct_damage_events(second);
    first.damage_amounts[0] = 999;
    auto const& queued{registry_.get_direct_damage_queue_view()};
    ml::simulation_tests::expect_equal(queued.num(), 3, "Damage batches append");
    ml::simulation_tests::expect_true(
        std::ranges::equal(queued.damage_amounts, std::array<std::int32_t, 3>{5, 8, 13}),
        "Damage order and values preserved");
    ml::simulation_tests::expect_true(
        std::ranges::equal(queued.damaged_entities, std::array{handles[1], handles[0], handles[1]}),
        "Damage victims preserved");
    ml::simulation_tests::expect_true(
        std::ranges::equal(queued.instigators,
                           std::array{handles[0], FRegistryEntityHandle{}, handles[0]}),
        "Damage instigators preserved");
    registry_.commit_updates();
    ml::simulation_tests::expect_equal(queued.num(), 3, "Commit retains damage queue");
    registry_.end_tick();
    ml::simulation_tests::expect_equal(queued.num(), 0, "End tick clears damage queue");
    auto pending{data};
    pending.alive[1] = 0;
    EntityDeathInfo deaths;
    deaths.add(ml::simulation::DeathReason::Combat, handles[1], handles[0]);
    registry_.queue_entity_updates({handles, view_of(pending)}, deaths);
    registry_.commit_updates();
    registry_.queue_direct_damage_events(second);
    registry_.reset();
    ml::simulation_tests::expect_equal(registry_.get_num_elements(), 0, "Reset clears slots");
    ml::simulation_tests::expect_equal(
        registry_.get_num_unique_ids_issued(), 0, "Reset clears history");
    ml::simulation_tests::expect_equal(registry_.count_kills(), 0, "Reset clears kills");
    ml::simulation_tests::expect_equal(queued.num(), 0, "Reset clears damage");
    ml::simulation_tests::expect_equal(
        static_cast<std::int32_t>(registry_.get_dead_entities_this_frame().size()),
        0,
        "Reset clears deaths");
    check_counts();
    auto const empty{registry_.add_entities(view_of(data, 0, 0))};
    ml::simulation_tests::expect_equal(
        empty.registry_handles.num(), 0, "Empty spawn returns no handles");
    ml::simulation_tests::expect_false(empty.first_id.is_valid(), "Empty spawn has null first ID");
    auto const spawned{registry_.add_entities(view_of(data))};
    ml::simulation_tests::expect_equal(spawned.first_id.id, 0, "Reset restarts IDs");
    ml::simulation_tests::expect_true(ml::registry_tests::make_handles(spawned.registry_handles) ==
                                          handles,
                                      "Reset restarts generations");
    registry_.commit_updates();
    check_row(data, 1, handles[1]);
    ml::simulation_tests::expect_equal(
        registry_.count_kills(), 0, "Reset discards pending death accounting");
    auto const& counters{registry_.get_combat_telemetry()};
    ml::simulation_tests::expect_equal(
        counters.hits[static_cast<std::int32_t>(ml::simulation::Team::Red)]
                     [static_cast<std::int32_t>(ml::simulation::EntityType::Turret)],
        std::uint64_t{0},
        "Reset discards previous hits");
    check_counts();
    registry_.end_tick();
}
