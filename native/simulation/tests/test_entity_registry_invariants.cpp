#include <ioj/sim/entity_registry.h>
#include "support/simulation_test_support.h"

namespace ioj::sim::tests {

namespace registry {
auto make_handles(RegistryEntityHandles const& source) -> std::vector<RegistryEntityHandle> {
    auto const count{source.num()};
    std::vector<RegistryEntityHandle> handles{};
    handles.reserve(static_cast<std::size_t>(count));
    for (std::int32_t index{}; index < count; ++index) {
        handles.push_back({source.registry_indices[index], source.generations[index]});
    }
    return handles;
}

auto make_entities(std::int32_t const count, std::int32_t const offset = 0)
    -> EntityRegistry::EntityData {
    EntityRegistry::EntityData data;
    data.add_defaulted(count);
    for (std::int32_t index{}; index < count; ++index) {
        auto const value{static_cast<float>(offset + index + 1)};
        data.locations.set(index, ml::make_vector3f(value, value + 1.f, value + 2.f));
        data.velocities.set(index, ml::make_vector3f(value + 3.f, value + 4.f, value + 5.f));
        data.rotations.set(index, {value + 6.f, value + 7.f, value + 8.f});
        data.healths[index] = offset + index + 100;
        data.teams[index] = index % 2 == 0 ? Team::Red : Team::Blue;
        data.entity_types[index] = index % 2 == 0 ? EntityType::Turret : EntityType::CapitalShip;
    }
    return data;
}
}

class EntityRegistryTest : public ::testing::Test {
  protected:
    EntityRegistry registry_;

    static auto view_of(EntityRegistry::EntityData const& data,
                        std::int32_t const offset = 0,
                        std::int32_t count = -1) -> EntityRegistry::EntityData::ConstView {
        if (count == -1) {
            count = data.num() - offset;
        }
        return data.get_const_view(offset, count);
    }

    void check_counts() {
        auto const& data{registry_.get_entity_data()};
        EntityRegistry::EntityCounts expected{};
        std::int32_t total{};
        auto const count{data.num()};
        for (std::int32_t slot{}; slot < count; ++slot) {
            if (is_alive(data.healths[slot])) {
                ++expected[static_cast<std::int32_t>(data.teams[slot])]
                          [static_cast<std::int32_t>(data.entity_types[slot])];
                ++total;
            }
        }
        tests::expect_equal(registry_.count_alive(), total, "Alive total");
        tests::expect_equal(registry_.count_alive_not_on_team(Team::COUNT),
                            total,
                            "Sentinel team excludes no entities");
        tests::expect_equal(registry_.count_alive_not_on_team(static_cast<Team>(255)),
                            total,
                            "Out-of-range team excludes no entities");
        tests::expect_equal(registry_.get_num_alive_active_entities(), total, "Active alive total");
        auto const actual{registry_.count_alive_per_team_and_type()};
        auto const team_counts{registry_.count_alive_per_team()};
        constexpr auto team_count{static_cast<std::int32_t>(Team::COUNT)};
        constexpr auto type_count{static_cast<std::int32_t>(EntityType::COUNT)};
        for (std::int32_t team{}; team < team_count; ++team) {
            std::int32_t team_total{};
            for (std::int32_t type{}; type < type_count; ++type) {
                tests::expect_equal(
                    actual[team][type], expected[team][type], "Team/type alive count");
                team_total += expected[team][type];
            }
            tests::expect_equal(team_counts[team], team_total, "Team alive count");
            tests::expect_equal(registry_.count_alive_not_on_team(static_cast<Team>(team)),
                                total - team_total,
                                "Other teams alive count");
        }
        for (std::int32_t type{}; type < type_count; ++type) {
            std::int32_t type_total{};
            for (std::int32_t team{}; team < team_count; ++team) {
                type_total += expected[team][type];
            }
            tests::expect_equal(registry_.count_alive(static_cast<EntityType>(type)),
                                type_total,
                                "Type alive count");
        }
    }

    void check_row(EntityRegistry::EntityData const& source,
                   std::int32_t const source_index,
                   RegistryEntityHandle const handle) {
        auto const& actual{registry_.get_entity_data()};
        auto const slot{handle.index};
        tests::expect_equal(actual.locations[slot], source.locations[source_index], "Location");
        tests::expect_equal(actual.velocities[slot], source.velocities[source_index], "Velocity");
        tests::expect_equal(
            actual.rotations.pitches[slot], source.rotations.pitches[source_index], "Pitch");
        tests::expect_equal(
            actual.rotations.yaws[slot], source.rotations.yaws[source_index], "Yaw");
        tests::expect_equal(
            actual.rotations.rolls[slot], source.rotations.rolls[source_index], "Roll");
        tests::expect_equal(actual.healths[slot], source.healths[source_index], "Health");
        tests::expect_true(actual.teams[slot] == source.teams[source_index], "Team");
        tests::expect_true(actual.entity_types[slot] == source.entity_types[source_index], "Type");
        auto const id{registry_.find_unique_id(handle)};
        auto const index{registry_.get_history_index(id)};
        auto const& history{registry_.get_unique_entities()};
        tests::expect_equal(history.registry_indices[index], slot, "Historical slot");
        tests::expect_equal(
            history.registry_generations[index], handle.generation, "Historical generation");
        tests::expect_equal(history.life_state[index] == LifeState::Alive,
                            is_alive(actual.healths[slot]),
                            "Historical alive");
        tests::expect_true(history.entity_types[index] == actual.entity_types[slot],
                           "Historical type");
        tests::expect_true(id.entity_type() == actual.entity_types[slot], "ID type");
        tests::expect_true(history.teams[index] == actual.teams[slot], "Historical team");
    }
};

TEST_F(EntityRegistryTest, MixedSlotReusePreservesDataAndHistoricalIdentity) {

    auto initial{tests::registry::make_entities(4)};
    auto const old_handles{
        tests::registry::make_handles(registry_.add_entities(view_of(initial)).registry_handles)};
    initial.healths[0] = 0;
    initial.healths[2] = 0;
    EntityDeathInfo deaths;
    deaths.add(DeathReason::Combat, old_handles[0], registry_.find_unique_id(old_handles[1]));
    deaths.add(DeathReason::Unknown, old_handles[2], {});
    registry_.queue_entity_updates({old_handles, view_of(initial)}, deaths);
    registry_.commit_updates();
    check_counts();
    tests::expect_true(registry_.analyse_handle(old_handles[0]) == RegistryHandleState::Active,
                       "Dead handle still active before reuse");
    registry_.end_tick();

    auto const added{tests::registry::make_entities(3, 20)};
    auto const spawned{registry_.add_entities(view_of(added))};
    auto const handles{tests::registry::make_handles(spawned.registry_handles)};
    tests::expect_equal(
        registry_.get_history_index(spawned.get_id(0)), 4, "History appends densely");
    tests::expect_true(spawned.get_id(0).entity_type() == added.entity_types[0],
                       "First ID embeds type");
    tests::expect_equal(registry_.get_num_elements(), 5, "Only remainder appended");
    tests::expect_true(handles[0] == RegistryEntityHandle{2, 1},
                       "Free slots are consumed from tail");
    tests::expect_true(handles[1] == RegistryEntityHandle{0, 1}, "Next free slot");
    tests::expect_true(handles[2] == RegistryEntityHandle{4, 0},
                       "Appended slot starts generation zero");
    for (std::int32_t index{}; index < 3; ++index) {
        check_row(added, index, handles[index]);
        auto const id{registry_.find_unique_id(handles[index])};
        tests::expect_equal(
            registry_.get_history_index(id), 4 + index, "History rows follow input order");
        tests::expect_true(id.entity_type() == added.entity_types[index], "ID embeds input type");
    }
    for (auto const slot : {0, 2}) {
        tests::expect_true(registry_.is_stale(old_handles[slot]), "Old handle becomes stale");
        tests::expect_false(registry_.is_valid_handle(old_handles[slot]),
                            "Old handle invalid for current data");
        auto const old_id{registry_.find_unique_id(old_handles[slot])};
        tests::expect_equal(
            registry_.get_history_index(old_id), slot, "Old handle resolves historical ID");
        tests::expect_true(old_id.entity_type() == initial.entity_types[slot],
                           "Historical ID retains type");
    }
    auto const& history{registry_.get_unique_entities()};
    tests::expect_true(history.life_state[0] == LifeState::Combat, "Death reason survives reuse");
    tests::expect_equal(
        registry_.get_history_index(history.killed_by[0]), 1, "Killer survives reuse");
    tests::expect_false(history.killed_by[2].is_valid(), "Unattributed death has no killer");
    tests::expect_true(history.life_state[0] != LifeState::Alive, "Historical victim stays dead");
    check_row(initial, 1, old_handles[1]);
    check_row(initial, 3, old_handles[3]);
    check_counts();
    registry_.end_tick();
}

TEST_F(EntityRegistryTest, QueuedUpdateBatchesConsolidateEachEntityAndOnlyChangeMutableFields) {

    auto initial{tests::registry::make_entities(3)};
    initial.healths[2] = 0;
    auto const handles{
        tests::registry::make_handles(registry_.add_entities(view_of(initial)).registry_handles)};
    check_counts();
    auto first{tests::registry::make_entities(2, 30)};
    first.teams[1] = Team::Green;
    std::vector const first_handles{handles[0], handles[2]};
    registry_.queue_entity_updates({first_handles, view_of(first)}, {});
    auto second{tests::registry::make_entities(1, 50)};
    second.teams[0] = Team::Yellow;
    std::vector const second_handles{handles[1]};
    registry_.queue_entity_updates({second_handles, view_of(second)}, {});
    tests::expect_equal(
        registry_.get_health(handles[0]), initial.healths[0], "Queue does not mutate live health");
    registry_.commit_updates();
    tests::expect_equal(
        registry_.get_health(handles[0]), first.healths[0], "First batch updates first entity");
    tests::expect_equal(
        registry_.get_health(handles[1]), second.healths[0], "Second batch updates other entity");
    tests::expect_equal(
        registry_.get_health(handles[2]), first.healths[1], "Earlier batch updates third entity");
    tests::expect_equal(registry_.get_location(handles[0]),
                        first.locations[0],
                        "Location uses consolidated update");
    tests::expect_equal(
        registry_.get_velocity(handles[2]), first.velocities[1], "Velocity uses last update");
    tests::expect_equal(registry_.get_entity_data().rotations.yaws[0],
                        first.rotations.yaws[0],
                        "Rotation uses last update");
    for (std::int32_t index{}; index < 3; ++index) {
        tests::expect_true(registry_.get_entity_type(handles[index]) == initial.entity_types[index],
                           "Type is spawn data");
        tests::expect_true(registry_.get_unique_entities().life_state[index] == LifeState::Alive,
                           "Alive synchronized");
    }
    check_counts();
    registry_.end_tick();
    registry_.commit_updates();
    tests::expect_equal(static_cast<std::int32_t>(registry_.get_dead_entities_this_frame().size()),
                        0,
                        "Cleanup leaves no death events");
    check_counts();
}

TEST_F(EntityRegistryTest, MovementReportsOnlyTransformChanges) {

    auto initial{tests::registry::make_entities(4)};
    auto const handles{
        tests::registry::make_handles(registry_.add_entities(view_of(initial)).registry_handles)};
    tests::expect_equal(static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
                        0,
                        "Spawn is not movement");

    registry_.queue_entity_updates({handles, view_of(initial)}, {});
    registry_.commit_updates();
    tests::expect_equal(static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
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
    tests::expect_equal(
        static_cast<std::int32_t>(moved.size()), 3, "Only transformed entities move");
    tests::expect_true(moved[0] == handles[0], "Position-only turret moves");
    tests::expect_true(moved[1] == handles[1], "Rotation-only capital moves");
    tests::expect_true(moved[2] == handles[3], "Combined transform moves");
    tests::expect_false(std::ranges::contains(moved, handles[2]),
                        "Health-only change is not movement");

    registry_.end_tick();
    tests::expect_equal(static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
                        3,
                        "Movement remains available after end tick");
    registry_.begin_tick();
    tests::expect_equal(static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
                        0,
                        "Begin tick clears movement");
}

TEST_F(EntityRegistryTest, RepeatedLocalMovementProducesOneConsolidatedRegistryUpdate) {

    auto const initial{tests::registry::make_entities(2)};
    auto const handles{
        tests::registry::make_handles(registry_.add_entities(view_of(initial)).registry_handles)};

    auto first{initial};
    first.locations.xs[0] += 20.f;
    first.locations.xs[0] += 20.f;
    first.rotations.yaws[0] += 40.f;
    registry_.queue_entity_updates({std::vector{handles[0]}, view_of(first, 0, 1)}, {});
    registry_.commit_updates();

    auto moved{registry_.get_moved_entities_this_tick()};
    tests::expect_equal(
        static_cast<std::int32_t>(moved.size()), 1, "Consolidated update reports one movement");
    tests::expect_true(moved[0] == handles[0], "Consolidated movement reports the handle");

    auto second{first};
    second.locations.xs[0] += 10.f;
    registry_.queue_entity_updates({std::vector{handles[0]}, view_of(second, 0, 1)}, {});
    registry_.commit_updates();
    tests::expect_equal(static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
                        1,
                        "A second commit does not duplicate movement");
    tests::expect_equal(registry_.get_location(handles[0]),
                        second.locations[0],
                        "A second commit publishes the latest row");
    registry_.end_tick();
    registry_.begin_tick();
}

TEST_F(EntityRegistryTest, MovementIsPerTickAndGenerationSafeAcrossSlotReuse) {

    auto data{tests::registry::make_entities(1)};
    auto const old_handle{registry_.add_entities(view_of(data)).get_handle(0)};

    data.locations.ys[0] += 10.f;
    registry_.queue_entity_updates({std::vector{old_handle}, view_of(data)}, {});
    registry_.commit_updates();
    auto moved{registry_.get_moved_entities_this_tick()};
    tests::expect_true(static_cast<std::int32_t>(moved.size()) == 1 && moved[0] == old_handle,
                       "First tick reports movement");
    registry_.end_tick();
    registry_.begin_tick();

    data.healths[0] = 0;
    EntityDeathInfo deaths;
    deaths.add(DeathReason::Unknown, old_handle, {});
    registry_.queue_entity_updates({std::vector{old_handle}, view_of(data)}, deaths);
    registry_.commit_updates();
    tests::expect_equal(static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
                        0,
                        "Destruction without transform change is not movement");
    registry_.end_tick();
    registry_.begin_tick();

    auto replacement_data{tests::registry::make_entities(1, 20)};
    auto const replacement{registry_.add_entities(view_of(replacement_data)).get_handle(0)};
    tests::expect_true(replacement.index == old_handle.index &&
                           replacement.generation != old_handle.generation,
                       "Replacement reuses the slot with a new generation");
    tests::expect_true(registry_.is_stale(old_handle), "Old moved handle is stale");
    tests::expect_equal(static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
                        0,
                        "Replacement spawn is not stale movement");

    replacement_data.locations.zs[0] += 5.f;
    registry_.queue_entity_updates({std::vector{replacement}, view_of(replacement_data)}, {});
    registry_.commit_updates();
    moved = registry_.get_moved_entities_this_tick();
    tests::expect_true(static_cast<std::int32_t>(moved.size()) == 1 && moved[0] == replacement,
                       "Replacement movement uses its current handle");
    registry_.end_tick();
    registry_.begin_tick();

    replacement_data.rotations.pitches[0] += 30.f;
    registry_.queue_entity_updates({std::vector{replacement}, view_of(replacement_data)}, {});
    registry_.commit_updates();
    moved = registry_.get_moved_entities_this_tick();
    tests::expect_true(static_cast<std::int32_t>(moved.size()) == 1 && moved[0] == replacement,
                       "Successive tick reports movement again");
    registry_.end_tick();
    registry_.begin_tick();

    registry_.queue_entity_updates({std::vector{replacement}, view_of(replacement_data)}, {});
    registry_.commit_updates();
    tests::expect_equal(static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
                        0,
                        "Previous movement does not leak into a quiet tick");
    registry_.end_tick();
    registry_.begin_tick();

    replacement_data.locations.xs[0] += 1.f;
    registry_.queue_entity_updates({std::vector{replacement}, view_of(replacement_data)}, {});
    registry_.commit_updates();
    tests::expect_equal(static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
                        1,
                        "Movement exists before reset");
    registry_.reset();
    tests::expect_equal(static_cast<std::int32_t>(registry_.get_moved_entities_this_tick().size()),
                        0,
                        "Registry reset clears movement");
}

TEST_F(EntityRegistryTest, FreeSlotsBecomeReusableAtEndTickAndGenerationsAdvanceEachReuse) {

    auto data{tests::registry::make_entities(1)};
    data.healths[0] = 0;
    auto const first{registry_.add_entities(view_of(data)).get_handle(0)};
    check_counts();
    EntityDeathInfo deaths;
    deaths.add(DeathReason::Unknown, first, {});
    registry_.queue_entity_updates({{}, view_of(data, 0, 0)}, deaths);
    registry_.commit_updates();
    data.healths[0] = 100;
    auto const before_cleanup{registry_.add_entities(view_of(data)).get_handle(0)};
    tests::expect_true(before_cleanup == RegistryEntityHandle{1, 0},
                       "Dead slot is not free until cleanup");
    registry_.end_tick();
    registry_.add_entities(view_of(data, 0, 0));
    auto const reused{registry_.add_entities(view_of(data)).get_handle(0)};
    tests::expect_true(reused == RegistryEntityHandle{0, 1},
                       "Empty spawn did not consume free slot");
    data.healths[0] = 0;
    deaths.reset();
    deaths.add(DeathReason::Unknown, reused, {});
    registry_.queue_entity_updates({std::vector{reused}, view_of(data)}, deaths);
    registry_.commit_updates();
    registry_.end_tick();
    data.healths[0] = 100;
    auto const next{registry_.add_entities(view_of(data)).get_handle(0)};
    tests::expect_true(next == RegistryEntityHandle{0, 2}, "Second reuse increments again");
    tests::expect_equal(registry_.get_history_index(registry_.find_unique_id(first)),
                        0,
                        "First occupant retains ID");
    tests::expect_equal(registry_.get_history_index(registry_.find_unique_id(reused)),
                        2,
                        "Second occupant retains ID");
    tests::expect_equal(registry_.get_history_index(registry_.find_unique_id(next)),
                        3,
                        "Third occupant receives new ID");
    check_counts();
    registry_.end_tick();
}

TEST_F(EntityRegistryTest, UniqueIdValidationRejectsInvalidUnissuedAndMismatchedIds) {

    auto const first_id{
        EntityUniqueId::make(entity_identity_offset(EntityType::Turret, 0), EntityType::Turret)};
    tests::expect_false(registry_.is_valid_unique_id(EntityUniqueId{std::uint32_t{0xffffffff}}),
                        "Out-of-domain raw ID rejected in empty registry");
    tests::expect_false(registry_.is_valid_unique_id(first_id), "Zero is initially unissued");
    auto const data{tests::registry::make_entities(1)};
    registry_.add_entities(view_of(data));
    tests::expect_false(registry_.is_valid_unique_id(EntityUniqueId{std::uint32_t{0xffffffff}}),
                        "Out-of-domain raw ID rejected after spawn");
    tests::expect_false(registry_.is_valid_unique_id({}), "Null ID rejected");
    tests::expect_false(registry_.is_valid_unique_id(EntityUniqueId::make(
                            entity_identity_offset(EntityType::Turret, 1), EntityType::Turret)),
                        "Next ID is unissued");
    tests::expect_false(
        registry_.is_valid_unique_id(EntityUniqueId::make(0, EntityType::PlayerShip)),
        "Mismatched type rejected");
    tests::expect_true(registry_.is_valid_unique_id(first_id), "Issued ID accepted");
}

TEST_F(EntityRegistryTest, TeamChangesSynchronizeHistoryAndSubsequentCombatAttribution) {

    auto data{tests::registry::make_entities(2)};
    auto const handles{
        tests::registry::make_handles(registry_.add_entities(view_of(data)).registry_handles)};
    registry_.record_shots(std::vector{registry_.get_current_id(handles[0])});
    data.teams[0] = Team::Green;
    data.teams[1] = Team::Yellow;
    registry_.queue_entity_updates({handles, view_of(data)}, {});
    registry_.commit_updates();
    registry_.end_tick();
    check_row(data, 0, handles[0]);
    check_row(data, 1, handles[1]);
    check_counts();
    registry_.record_shots(std::vector{registry_.get_current_id(handles[0]), EntityUniqueId{}});
    DirectDamageEvents damage;
    damage.add(registry_.get_current_id(handles[1]), 17, registry_.find_unique_id(handles[0]));
    registry_.queue_direct_damage_events(damage);
    data.healths[1] = 0;
    EntityDeathInfo deaths;
    deaths.add(DeathReason::Combat, handles[1], registry_.find_unique_id(handles[0]));
    registry_.queue_entity_updates({handles, view_of(data)}, deaths);
    registry_.commit_updates();
    auto const& counters{registry_.get_combat_telemetry()};
    auto const red{static_cast<std::int32_t>(Team::Red)};
    auto const green{static_cast<std::int32_t>(Team::Green)};
    auto const yellow{static_cast<std::int32_t>(Team::Yellow)};
    auto const turret{static_cast<std::int32_t>(EntityType::Turret)};
    auto const capital{static_cast<std::int32_t>(EntityType::CapitalShip)};
    tests::expect_equal(
        counters.shots[red][turret], std::uint64_t{1}, "Earlier shot retains original team");
    tests::expect_equal(
        counters.shots[green][turret], std::uint64_t{1}, "Later shot uses committed team");
    tests::expect_equal(counters.hits[green][turret], std::uint64_t{1}, "Hit uses committed team");
    tests::expect_equal(
        counters.damage_dealt[green][turret], 17.0, "Damage dealt uses committed team");
    tests::expect_equal(
        counters.damage_received[yellow][capital], 17.0, "Damage received uses committed team");
    tests::expect_equal(
        counters.kills[green][turret], std::uint64_t{1}, "Kill uses committed team");
    tests::expect_equal(
        counters.losses[yellow][capital], std::uint64_t{1}, "Loss uses committed team");
    tests::expect_equal(
        counters.destroyed[yellow][capital], std::uint64_t{1}, "Destruction uses committed team");
    tests::expect_equal(
        counters.kill_matrix[green][yellow], std::uint64_t{1}, "Kill matrix uses committed teams");
    registry_.end_tick();
    auto const replacement{tests::registry::make_entities(1)};
    registry_.add_entities(view_of(replacement));
    tests::expect_true(registry_.get_unique_entities().teams[1] == Team::Yellow,
                       "Reused slot preserves victim's last team");
}

TEST_F(EntityRegistryTest, StaleKillersRetainCreditAcrossTicksAndSlotReuse) {

    auto data{tests::registry::make_entities(3)};
    auto const handles{
        tests::registry::make_handles(registry_.add_entities(view_of(data)).registry_handles)};
    auto const killer_id{registry_.get_current_id(handles[0])};
    data.healths[0] = 0;
    data.healths[1] = 0;
    EntityDeathInfo deaths;
    deaths.add(DeathReason::Combat, handles[1], killer_id);
    deaths.add(DeathReason::Unknown, handles[0], {});
    registry_.queue_entity_updates({handles, view_of(data)}, deaths);
    registry_.commit_updates();
    auto const dead{registry_.get_dead_entities_this_frame()};
    tests::expect_equal(static_cast<std::int32_t>(dead.size()), 2, "Both deaths recorded");
    tests::expect_true(dead[0] == handles[1] && dead[1] == handles[0], "Death order follows queue");
    tests::expect_equal(registry_.count_kills(), 1, "Only attributed death counts as kill");
    registry_.end_tick();
    auto const replacement{tests::registry::make_entities(2, 40)};
    auto const replacements{tests::registry::make_handles(
        registry_.add_entities(view_of(replacement)).registry_handles)};
    tests::expect_true(registry_.is_stale(handles[0]), "Killer handle is stale");
    DirectDamageEvents damage;
    damage.add(registry_.get_current_id(handles[2]), 17, killer_id);
    registry_.queue_direct_damage_events(damage);
    EXPECT_EQ(registry_.get_direct_damage_queue_view().instigators[0], killer_id);
    auto update{tests::registry::make_entities(1)};
    update.healths[0] = 0;
    deaths.reset();
    deaths.add(DeathReason::Combat, handles[2], killer_id);
    registry_.queue_entity_updates({std::vector{handles[2]}, view_of(update)}, deaths);
    registry_.commit_updates();
    tests::expect_equal(registry_.get_kills(EntityUniqueId::make(
                            entity_identity_offset(EntityType::Turret, 0), EntityType::Turret)),
                        std::uint32_t{2},
                        "Historical killer gains credit");
    tests::expect_equal(registry_.get_history_index(registry_.get_unique_entities().killed_by[2]),
                        0,
                        "Victim records historical killer");
    tests::expect_equal(registry_.get_kills(registry_.find_unique_id(replacements[1])),
                        std::uint32_t{0},
                        "Replacement gains no credit");
    tests::expect_equal(registry_.count_kills(), 2, "Kills accumulate across ticks");
    check_counts();
    registry_.end_tick();
    registry_.commit_updates();
    tests::expect_equal(static_cast<std::int32_t>(registry_.get_dead_entities_this_frame().size()),
                        0,
                        "Deaths cleared at end tick");
    tests::expect_equal(registry_.count_kills(), 2, "Cleared deaths are not replayed");
}

TEST_F(EntityRegistryTest, RefreshDistinguishesNullStaleDeadAndLiveHandles) {

    auto data{tests::registry::make_entities(3)};
    auto const handles{
        tests::registry::make_handles(registry_.add_entities(view_of(data)).registry_handles)};
    tests::expect_true(registry_.analyse_handle({}) == RegistryHandleState::Null,
                       "Default is null");
    tests::expect_true(registry_.analyse_handle({3, 0}) == RegistryHandleState::Invalid,
                       "Out of range is invalid");
    tests::expect_true(registry_.analyse_handle({0, 1}) == RegistryHandleState::Invalid,
                       "Future generation is invalid");
    data.healths[0] = 0;
    EntityDeathInfo deaths;
    deaths.add(DeathReason::Unknown, handles[0], {});
    registry_.queue_entity_updates({handles, view_of(data)}, deaths);
    registry_.commit_updates();
    registry_.end_tick();
    registry_.add_entities(view_of(data, 1, 1));
    data.healths[1] = 0;
    deaths.reset();
    deaths.add(DeathReason::Unknown, handles[1], {});
    registry_.queue_entity_updates({std::vector{handles[1]}, view_of(data, 1, 1)}, deaths);
    registry_.commit_updates();
    std::vector refreshed{RegistryEntityHandle{}, handles[0], handles[1], handles[2]};
    Vectors3f locations;
    Vectors3f velocities;
    locations.add_defaulted(4);
    velocities.add_defaulted(4);
    registry_.refresh_entity_data(refreshed, locations.get_view(), velocities.get_view());
    for (std::int32_t index{}; index < 3; ++index) {
        tests::expect_true(refreshed[index].is_null(), "Null, stale and dead become null");
        tests::expect_equal(locations[index], ml::make_vector3f(0.f, 0.f, 0.f), "Cleared location");
        tests::expect_equal(
            velocities[index], ml::make_vector3f(0.f, 0.f, 0.f), "Cleared velocity");
    }
    tests::expect_true(refreshed[3] == handles[2], "Live handle retained");
    tests::expect_equal(locations[3], data.locations[2], "Live location refreshed");
    tests::expect_equal(velocities[3], data.velocities[2], "Live velocity refreshed");
    registry_.refresh_entity_data(refreshed, {}, {});
    registry_.refresh_entity_data(refreshed, {}, velocities.get_view());
    registry_.refresh_entity_data(refreshed, locations.get_view(), {});
    registry_.refresh_entity_data({}, {}, {});
    std::vector const location_handles{RegistryEntityHandle{}, handles[1]};
    Vectors3f dead_locations;
    dead_locations.add_defaulted(2);
    registry_.refresh_locations(location_handles, dead_locations.get_view());
    tests::expect_equal(
        dead_locations[1], data.locations[1], "Location-only refresh accepts current dead handle");
    registry_.end_tick();
}

TEST_F(EntityRegistryTest, DamageQueuesPreserveBatchesAndResetStartsANewIdentityLifetime) {

    auto const data{tests::registry::make_entities(2)};
    auto const handles{
        tests::registry::make_handles(registry_.add_entities(view_of(data)).registry_handles)};
    DirectDamageEvents first;
    first.add(registry_.get_current_id(handles[1]), 5, registry_.find_unique_id(handles[0]));
    first.add(registry_.get_current_id(handles[0]), 8, {});
    DirectDamageEvents second;
    second.add(registry_.get_current_id(handles[1]), 13, registry_.find_unique_id(handles[0]));
    registry_.queue_direct_damage_events(first);
    registry_.queue_direct_damage_events(second);
    first.damage_amounts[0] = 999;
    auto const& queued{registry_.get_direct_damage_queue_view()};
    tests::expect_equal(queued.num(), 3, "Damage batches append");
    tests::expect_true(
        std::ranges::equal(queued.damage_amounts, std::array<std::int32_t, 3>{5, 8, 13}),
        "Damage order and values preserved");
    tests::expect_true(std::ranges::equal(queued.damaged_entities,
                                          std::array{registry_.get_current_id(handles[1]),
                                                     registry_.get_current_id(handles[0]),
                                                     registry_.get_current_id(handles[1])}),
                       "Damage victims preserved");
    tests::expect_true(std::ranges::equal(queued.instigators,
                                          std::array{registry_.find_unique_id(handles[0]),
                                                     EntityUniqueId{},
                                                     registry_.find_unique_id(handles[0])}),
                       "Damage instigators preserved");
    SimClock clock;
    AgentIndexes indexes{clock};
    std::array const turret_ids{registry_.get_current_id(handles[0])};
    std::array const capital_ids{registry_.get_current_id(handles[1])};
    indexes.bind(EntityType::Turret, turret_ids);
    indexes.bind(EntityType::CapitalShip, capital_ids);
    std::pmr::monotonic_buffer_resource scratch;
    registry_.prepare_damage_events(indexes, scratch);
    auto const capital_damage{registry_.get_damage_events(EntityType::CapitalShip)};
    ASSERT_EQ(capital_damage.num(), 2);
    EXPECT_EQ(capital_damage.damage_amounts[0], 5);
    EXPECT_EQ(capital_damage.damage_amounts[1], 13);
    EXPECT_EQ(capital_damage.damaged_entities[0], capital_ids[0]);
    EXPECT_EQ(capital_damage.instigators[1], registry_.find_unique_id(handles[0]));
    auto const turret_damage{registry_.get_damage_events(EntityType::Turret)};
    ASSERT_EQ(turret_damage.num(), 1);
    EXPECT_EQ(turret_damage.damage_amounts[0], 8);
    EXPECT_TRUE(!turret_damage.instigators[0].is_valid());
    EXPECT_TRUE(registry_.get_damage_events(EntityType::Fighter).is_empty());
    registry_.commit_updates();
    tests::expect_equal(queued.num(), 3, "Commit retains damage queue");
    registry_.end_tick();
    tests::expect_equal(queued.num(), 0, "End tick clears damage queue");
    EXPECT_TRUE(registry_.get_damage_events(EntityType::CapitalShip).is_empty());
    auto pending{data};
    pending.healths[1] = 0;
    EntityDeathInfo deaths;
    deaths.add(DeathReason::Combat, handles[1], registry_.find_unique_id(handles[0]));
    registry_.queue_entity_updates({handles, view_of(pending)}, deaths);
    registry_.commit_updates();
    registry_.queue_direct_damage_events(second);
    indexes.retire(capital_ids[0]);
    registry_.prepare_damage_events(indexes, scratch);
    EXPECT_TRUE(registry_.get_damage_events(EntityType::CapitalShip).is_empty());
    EXPECT_EQ(queued.num(), 0);
    registry_.reset();
    tests::expect_equal(registry_.get_num_elements(), 0, "Reset clears slots");
    tests::expect_equal(registry_.get_num_unique_ids_issued(), 0, "Reset clears history");
    tests::expect_equal(registry_.count_kills(), 0, "Reset clears kills");
    tests::expect_equal(queued.num(), 0, "Reset clears damage");
    tests::expect_equal(static_cast<std::int32_t>(registry_.get_dead_entities_this_frame().size()),
                        0,
                        "Reset clears deaths");
    check_counts();
    auto const empty{registry_.add_entities(view_of(data, 0, 0))};
    tests::expect_equal(empty.registry_handles.num(), 0, "Empty spawn returns no handles");
    tests::expect_true(empty.entity_ids.empty(), "Empty spawn has no IDs");
    auto const spawned{registry_.add_entities(view_of(data))};
    tests::expect_equal(spawned.get_id(0).index(),
                        entity_identity_offsets[data.entity_types[0]],
                        "Reset restarts IDs");
    tests::expect_true(tests::registry::make_handles(spawned.registry_handles) == handles,
                       "Reset restarts generations");
    registry_.commit_updates();
    check_row(data, 1, handles[1]);
    tests::expect_equal(registry_.count_kills(), 0, "Reset discards pending death accounting");
    auto const& counters{registry_.get_combat_telemetry()};
    tests::expect_equal(counters.hits[static_cast<std::int32_t>(Team::Red)]
                                     [static_cast<std::int32_t>(EntityType::Turret)],
                        std::uint64_t{0},
                        "Reset discards previous hits");
    check_counts();
    registry_.end_tick();
}

} // namespace tests
