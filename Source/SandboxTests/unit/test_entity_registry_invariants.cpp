#include <sandbox/simulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>

#include <CQTest.h>

#include <algorithm>
#include <array>

namespace ml::registry_tests {
auto make_handles(RegistryEntityHandles const& source) -> std::vector<FRegistryEntityHandle> {
    auto const count{source.num()};
    std::vector<FRegistryEntityHandle> handles;
    handles.reserve(static_cast<std::size_t>(count));
    for (int32 index{}; index < count; ++index) {
        handles.push_back({source.registry_indices[index], source.generations[index]});
    }
    return handles;
}

auto make_entities(int32 const count, int32 const offset = 0) -> FTestEntityRegistry::EntityData {
    FTestEntityRegistry::EntityData data;
    data.add_defaulted(count);
    for (int32 index{}; index < count; ++index) {
        auto const value{static_cast<float>(offset + index + 1)};
        data.locations.set(index, ml::make_vector3f(value, value + 1.f, value + 2.f));
        data.velocities.set(index, ml::make_vector3f(value + 3.f, value + 4.f, value + 5.f));
        data.rotations.set(index, {value + 6.f, value + 7.f, value + 8.f});
        data.radii[index] = value + 9.f;
        data.healths[index] = offset + index + 100;
        data.teams[index] = index % 2 == 0 ? ml::simulation::Team::Red : ml::simulation::Team::Blue;
        data.entity_types[index] = index % 2 == 0 ? ml::simulation::EntityType::Turret
                                                  : ml::simulation::EntityType::CapitalShip;
        data.alive[index] = 1;
    }
    return data;
}
}

TEST_CLASS(EntityRegistry, "Sandbox.UnitTests")
{
    FTestEntityRegistry registry_;

    static auto view_of(FTestEntityRegistry::EntityData const& data,
                        int32 const offset = 0,
                        int32 count = INDEX_NONE) -> FTestEntityRegistry::EntityData::ConstView {
        if (count == INDEX_NONE) {
            count = data.num() - offset;
        }
        return data.get_const_view(offset, count);
    }

    void check_counts() {
        auto const& data{registry_.get_entity_data()};
        FTestEntityRegistry::EntityCounts expected{};
        int32 total{};
        auto const count{data.num()};
        for (int32 slot{}; slot < count; ++slot) {
            if (data.alive[slot] != 0) {
                ++expected[static_cast<int32>(data.teams[slot])]
                          [static_cast<int32>(data.entity_types[slot])];
                ++total;
            }
        }
        TestRunner->TestEqual(TEXT("Alive total"), registry_.count_alive(), total);
        TestRunner->TestEqual(TEXT("Sentinel team excludes no entities"),
                              registry_.count_alive_not_on_team(ml::simulation::Team::COUNT),
                              total);
        TestRunner->TestEqual(
            TEXT("Out-of-range team excludes no entities"),
            registry_.count_alive_not_on_team(static_cast<ml::simulation::Team>(255)),
            total);
        TestRunner->TestEqual(
            TEXT("Active alive total"), registry_.get_num_alive_active_entities(), total);
        auto const actual{registry_.count_alive_per_team_and_type()};
        auto const team_counts{registry_.count_alive_per_team()};
        constexpr auto team_count{static_cast<int32>(ml::simulation::Team::COUNT)};
        constexpr auto type_count{static_cast<int32>(ml::simulation::EntityType::COUNT)};
        for (int32 team{}; team < team_count; ++team) {
            int32 team_total{};
            for (int32 type{}; type < type_count; ++type) {
                TestRunner->TestEqual(
                    TEXT("Team/type alive count"), actual[team][type], expected[team][type]);
                team_total += expected[team][type];
            }
            TestRunner->TestEqual(TEXT("Team alive count"), team_counts[team], team_total);
            TestRunner->TestEqual(
                TEXT("Other teams alive count"),
                registry_.count_alive_not_on_team(static_cast<ml::simulation::Team>(team)),
                total - team_total);
        }
        for (int32 type{}; type < type_count; ++type) {
            int32 type_total{};
            for (int32 team{}; team < team_count; ++team) {
                type_total += expected[team][type];
            }
            TestRunner->TestEqual(
                TEXT("Type alive count"),
                registry_.count_alive(static_cast<ml::simulation::EntityType>(type)),
                type_total);
        }
    }

    void check_row(FTestEntityRegistry::EntityData const& source,
                   int32 const source_index,
                   FRegistryEntityHandle const handle) {
        auto const& actual{registry_.get_entity_data()};
        auto const slot{handle.index};
        TestRunner->TestEqual(TEXT("Location"),
                              ml::to_unreal(actual.locations[slot]),
                              ml::to_unreal(source.locations[source_index]));
        TestRunner->TestEqual(TEXT("Velocity"),
                              ml::to_unreal(actual.velocities[slot]),
                              ml::to_unreal(source.velocities[source_index]));
        TestRunner->TestEqual(
            TEXT("Pitch"), actual.rotations.pitches[slot], source.rotations.pitches[source_index]);
        TestRunner->TestEqual(
            TEXT("Yaw"), actual.rotations.yaws[slot], source.rotations.yaws[source_index]);
        TestRunner->TestEqual(
            TEXT("Roll"), actual.rotations.rolls[slot], source.rotations.rolls[source_index]);
        TestRunner->TestEqual(TEXT("Radius"), actual.radii[slot], source.radii[source_index]);
        TestRunner->TestEqual(TEXT("Health"), actual.healths[slot], source.healths[source_index]);
        TestRunner->TestTrue(TEXT("Team"), actual.teams[slot] == source.teams[source_index]);
        TestRunner->TestTrue(TEXT("Type"),
                             actual.entity_types[slot] == source.entity_types[source_index]);
        TestRunner->TestEqual(TEXT("Alive"), actual.alive[slot], source.alive[source_index]);
        auto const id{registry_.find_unique_id(handle)};
        auto const& history{registry_.get_unique_entities()};
        TestRunner->TestEqual(TEXT("Historical slot"), history.registry_indices[id.id], slot);
        TestRunner->TestEqual(
            TEXT("Historical generation"), history.registry_generations[id.id], handle.generation);
        TestRunner->TestEqual(TEXT("Historical alive"), history.alive[id.id], actual.alive[slot]);
        TestRunner->TestTrue(TEXT("Historical type"),
                             history.entity_types[id.id] == actual.entity_types[slot]);
        TestRunner->TestTrue(TEXT("Historical team"), history.teams[id.id] == actual.teams[slot]);
    }

    TEST_METHOD(MixedSlotReusePreservesDataAndHistoricalIdentity)
    {
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
        TestRunner->TestTrue(TEXT("Dead handle still active before reuse"),
                             registry_.analyse_handle(old_handles[0]) ==
                                 ml::simulation::RegistryHandleState::Active);
        registry_.end_tick();

        auto const added{ml::registry_tests::make_entities(3, 20)};
        auto const spawned{registry_.add_entities(view_of(added))};
        auto const handles{ml::registry_tests::make_handles(spawned.registry_handles)};
        TestRunner->TestEqual(TEXT("New IDs start after history"), spawned.first_id.id, 4);
        TestRunner->TestEqual(TEXT("Only remainder appended"), registry_.get_num_elements(), 5);
        TestRunner->TestTrue(TEXT("Free slots are consumed from tail"),
                             handles[0] == FRegistryEntityHandle{2, 1});
        TestRunner->TestTrue(TEXT("Next free slot"), handles[1] == FRegistryEntityHandle{0, 1});
        TestRunner->TestTrue(TEXT("Appended slot starts generation zero"),
                             handles[2] == FRegistryEntityHandle{4, 0});
        for (int32 index{}; index < 3; ++index) {
            check_row(added, index, handles[index]);
            TestRunner->TestEqual(TEXT("IDs follow input order"),
                                  registry_.find_unique_id(handles[index]).id,
                                  4 + index);
        }
        for (auto const slot : {0, 2}) {
            TestRunner->TestTrue(TEXT("Old handle becomes stale"),
                                 registry_.is_stale(old_handles[slot]));
            TestRunner->TestFalse(TEXT("Old handle invalid for current data"),
                                  registry_.is_valid_handle(old_handles[slot]));
            TestRunner->TestEqual(TEXT("Old handle resolves historical ID"),
                                  registry_.find_unique_id(old_handles[slot]).id,
                                  slot);
        }
        auto const& history{registry_.get_unique_entities()};
        TestRunner->TestTrue(TEXT("Death reason survives reuse"),
                             history.death_reason[0] == ml::simulation::DeathReason::Combat);
        TestRunner->TestEqual(TEXT("Killer survives reuse"), history.killed_by[0].id, 1);
        TestRunner->TestFalse(TEXT("Unattributed death has no killer"),
                              history.killed_by[2].is_valid());
        TestRunner->TestEqual(TEXT("Historical victim stays dead"), history.alive[0], uint8{0});
        check_row(initial, 1, old_handles[1]);
        check_row(initial, 3, old_handles[3]);
        check_counts();
        registry_.end_tick();
    }

    TEST_METHOD(QueuedUpdateBatchesConsolidateEachEntityAndOnlyChangeMutableFields)
    {
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
        TestRunner->TestEqual(TEXT("Queue does not mutate live health"),
                              registry_.get_health(handles[0]),
                              initial.healths[0]);
        registry_.commit_updates();
        TestRunner->TestEqual(TEXT("First batch updates first entity"),
                              registry_.get_health(handles[0]),
                              first.healths[0]);
        TestRunner->TestEqual(TEXT("Second batch updates other entity"),
                              registry_.get_health(handles[1]),
                              second.healths[0]);
        TestRunner->TestEqual(TEXT("Earlier batch updates third entity"),
                              registry_.get_health(handles[2]),
                              first.healths[1]);
        TestRunner->TestEqual(TEXT("Location uses consolidated update"),
                              ml::to_unreal(registry_.get_location(handles[0])),
                              ml::to_unreal(first.locations[0]));
        TestRunner->TestEqual(TEXT("Velocity uses last update"),
                              ml::to_unreal(registry_.get_velocity(handles[2])),
                              ml::to_unreal(first.velocities[1]));
        TestRunner->TestEqual(TEXT("Rotation uses last update"),
                              registry_.get_entity_data().rotations.yaws[0],
                              first.rotations.yaws[0]);
        for (int32 index{}; index < 3; ++index) {
            TestRunner->TestEqual(TEXT("Radius is spawn data"),
                                  registry_.get_entity_data().radii[index],
                                  initial.radii[index]);
            TestRunner->TestTrue(TEXT("Type is spawn data"),
                                 registry_.get_entity_type(handles[index]) ==
                                     initial.entity_types[index]);
            TestRunner->TestEqual(
                TEXT("Alive synchronized"), registry_.get_unique_entities().alive[index], uint8{1});
        }
        check_counts();
        registry_.end_tick();
        registry_.commit_updates();
        TestRunner->TestEqual(TEXT("Cleanup leaves no death events"),
                              static_cast<int32>(registry_.get_dead_entities_this_frame().size()),
                              0);
        check_counts();
    }

    TEST_METHOD(MovementReportsOnlyTransformChanges)
    {
        auto initial{ml::registry_tests::make_entities(4)};
        auto const handles{ml::registry_tests::make_handles(
            registry_.add_entities(view_of(initial)).registry_handles)};
        TestRunner->TestEqual(TEXT("Spawn is not movement"),
                              static_cast<int32>(registry_.get_moved_entities_this_tick().size()),
                              0);

        registry_.queue_entity_updates({handles, view_of(initial)}, {});
        registry_.commit_updates();
        TestRunner->TestEqual(TEXT("Identical transforms do not move"),
                              static_cast<int32>(registry_.get_moved_entities_this_tick().size()),
                              0);
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
        TestRunner->TestEqual(
            TEXT("Only transformed entities move"), static_cast<int32>(moved.size()), 3);
        TestRunner->TestTrue(TEXT("Position-only turret moves"), moved[0] == handles[0]);
        TestRunner->TestTrue(TEXT("Rotation-only capital moves"), moved[1] == handles[1]);
        TestRunner->TestTrue(TEXT("Combined transform moves"), moved[2] == handles[3]);
        TestRunner->TestFalse(TEXT("Health-only change is not movement"),
                              std::ranges::contains(moved, handles[2]));

        registry_.end_tick();
        TestRunner->TestEqual(TEXT("Movement remains available after end tick"),
                              static_cast<int32>(registry_.get_moved_entities_this_tick().size()),
                              3);
        registry_.begin_tick();
        TestRunner->TestEqual(TEXT("Begin tick clears movement"),
                              static_cast<int32>(registry_.get_moved_entities_this_tick().size()),
                              0);
    }

    TEST_METHOD(RepeatedLocalMovementProducesOneConsolidatedRegistryUpdate)
    {
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
        TestRunner->TestEqual(
            TEXT("Consolidated update reports one movement"), static_cast<int32>(moved.size()), 1);
        TestRunner->TestTrue(TEXT("Consolidated movement reports the handle"),
                             moved[0] == handles[0]);

        auto second{first};
        second.locations.xs[0] += 10.f;
        registry_.queue_entity_updates({std::vector{handles[0]}, view_of(second, 0, 1)}, {});
        registry_.commit_updates();
        TestRunner->TestEqual(TEXT("A second commit does not duplicate movement"),
                              static_cast<int32>(registry_.get_moved_entities_this_tick().size()),
                              1);
        TestRunner->TestEqual(TEXT("A second commit publishes the latest row"),
                              ml::to_unreal(registry_.get_location(handles[0])),
                              ml::to_unreal(second.locations[0]));
        registry_.end_tick();
        registry_.begin_tick();
    }

    TEST_METHOD(MovementIsPerTickAndGenerationSafeAcrossSlotReuse)
    {
        auto data{ml::registry_tests::make_entities(1)};
        auto const old_handle{registry_.add_entities(view_of(data)).get_handle(0)};

        data.locations.ys[0] += 10.f;
        registry_.queue_entity_updates({std::vector{old_handle}, view_of(data)}, {});
        registry_.commit_updates();
        auto moved{registry_.get_moved_entities_this_tick()};
        TestRunner->TestTrue(TEXT("First tick reports movement"),
                             static_cast<int32>(moved.size()) == 1 && moved[0] == old_handle);
        registry_.end_tick();
        registry_.begin_tick();

        data.alive[0] = 0;
        EntityDeathInfo deaths;
        deaths.add(ml::simulation::DeathReason::Unknown, old_handle, {});
        registry_.queue_entity_updates({std::vector{old_handle}, view_of(data)}, deaths);
        registry_.commit_updates();
        TestRunner->TestEqual(TEXT("Destruction without transform change is not movement"),
                              static_cast<int32>(registry_.get_moved_entities_this_tick().size()),
                              0);
        registry_.end_tick();
        registry_.begin_tick();

        auto replacement_data{ml::registry_tests::make_entities(1, 20)};
        auto const replacement{registry_.add_entities(view_of(replacement_data)).get_handle(0)};
        TestRunner->TestTrue(TEXT("Replacement reuses the slot with a new generation"),
                             replacement.index == old_handle.index &&
                                 replacement.generation != old_handle.generation);
        TestRunner->TestTrue(TEXT("Old moved handle is stale"), registry_.is_stale(old_handle));
        TestRunner->TestEqual(TEXT("Replacement spawn is not stale movement"),
                              static_cast<int32>(registry_.get_moved_entities_this_tick().size()),
                              0);

        replacement_data.locations.zs[0] += 5.f;
        registry_.queue_entity_updates({std::vector{replacement}, view_of(replacement_data)}, {});
        registry_.commit_updates();
        moved = registry_.get_moved_entities_this_tick();
        TestRunner->TestTrue(TEXT("Replacement movement uses its current handle"),
                             static_cast<int32>(moved.size()) == 1 && moved[0] == replacement);
        registry_.end_tick();
        registry_.begin_tick();

        replacement_data.rotations.pitches[0] += 30.f;
        registry_.queue_entity_updates({std::vector{replacement}, view_of(replacement_data)}, {});
        registry_.commit_updates();
        moved = registry_.get_moved_entities_this_tick();
        TestRunner->TestTrue(TEXT("Successive tick reports movement again"),
                             static_cast<int32>(moved.size()) == 1 && moved[0] == replacement);
        registry_.end_tick();
        registry_.begin_tick();

        registry_.queue_entity_updates({std::vector{replacement}, view_of(replacement_data)}, {});
        registry_.commit_updates();
        TestRunner->TestEqual(TEXT("Previous movement does not leak into a quiet tick"),
                              static_cast<int32>(registry_.get_moved_entities_this_tick().size()),
                              0);
        registry_.end_tick();
        registry_.begin_tick();

        replacement_data.locations.xs[0] += 1.f;
        registry_.queue_entity_updates({std::vector{replacement}, view_of(replacement_data)}, {});
        registry_.commit_updates();
        TestRunner->TestEqual(TEXT("Movement exists before reset"),
                              static_cast<int32>(registry_.get_moved_entities_this_tick().size()),
                              1);
        registry_.reset();
        TestRunner->TestEqual(TEXT("Registry reset clears movement"),
                              static_cast<int32>(registry_.get_moved_entities_this_tick().size()),
                              0);
    }

    TEST_METHOD(FreeSlotsBecomeReusableAtEndTickAndGenerationsAdvanceEachReuse)
    {
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
        TestRunner->TestTrue(TEXT("Dead slot is not free until cleanup"),
                             before_cleanup == FRegistryEntityHandle{1, 0});
        registry_.end_tick();
        registry_.add_entities(view_of(data, 0, 0));
        auto const reused{registry_.add_entities(view_of(data)).get_handle(0)};
        TestRunner->TestTrue(TEXT("Empty spawn did not consume free slot"),
                             reused == FRegistryEntityHandle{0, 1});
        data.alive[0] = 0;
        deaths.reset();
        deaths.add(ml::simulation::DeathReason::Unknown, reused, {});
        registry_.queue_entity_updates({std::vector{reused}, view_of(data)}, deaths);
        registry_.commit_updates();
        registry_.end_tick();
        data.alive[0] = 1;
        auto const next{registry_.add_entities(view_of(data)).get_handle(0)};
        TestRunner->TestTrue(TEXT("Second reuse increments again"),
                             next == FRegistryEntityHandle{0, 2});
        TestRunner->TestEqual(
            TEXT("First occupant retains ID"), registry_.find_unique_id(first).id, 0);
        TestRunner->TestEqual(
            TEXT("Second occupant retains ID"), registry_.find_unique_id(reused).id, 2);
        TestRunner->TestEqual(
            TEXT("Third occupant receives new ID"), registry_.find_unique_id(next).id, 3);
        check_counts();
        registry_.end_tick();
    }

    TEST_METHOD(UniqueIdValidationRejectsNegativeAndUnissuedIds)
    {
        TestRunner->TestFalse(TEXT("Negative ID rejected in empty registry"),
                              registry_.is_valid_unique_id({-1}));
        TestRunner->TestFalse(TEXT("Zero is initially unissued"),
                              registry_.is_valid_unique_id({0}));
        auto const data{ml::registry_tests::make_entities(1)};
        registry_.add_entities(view_of(data));
        TestRunner->TestFalse(TEXT("Negative ID rejected after spawn"),
                              registry_.is_valid_unique_id({-1}));
        TestRunner->TestFalse(TEXT("Null ID rejected"), registry_.is_valid_unique_id({}));
        TestRunner->TestFalse(TEXT("Next ID is unissued"), registry_.is_valid_unique_id({1}));
        TestRunner->TestTrue(TEXT("Issued ID accepted"), registry_.is_valid_unique_id({0}));
    }

    TEST_METHOD(TeamChangesSynchronizeHistoryAndSubsequentCombatAttribution)
    {
        auto data{ml::registry_tests::make_entities(2)};
        auto const handles{ml::registry_tests::make_handles(
            registry_.add_entities(view_of(data)).registry_handles)};
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
        auto const red{static_cast<int32>(ml::simulation::Team::Red)};
        auto const green{static_cast<int32>(ml::simulation::Team::Green)};
        auto const yellow{static_cast<int32>(ml::simulation::Team::Yellow)};
        auto const turret{static_cast<int32>(ml::simulation::EntityType::Turret)};
        auto const capital{static_cast<int32>(ml::simulation::EntityType::CapitalShip)};
        TestRunner->TestEqual(
            TEXT("Earlier shot retains original team"), counters.shots[red][turret], uint64{1});
        TestRunner->TestEqual(
            TEXT("Later shot uses committed team"), counters.shots[green][turret], uint64{1});
        TestRunner->TestEqual(
            TEXT("Hit uses committed team"), counters.hits[green][turret], uint64{1});
        TestRunner->TestEqual(
            TEXT("Damage dealt uses committed team"), counters.damage_dealt[green][turret], 17.0);
        TestRunner->TestEqual(TEXT("Damage received uses committed team"),
                              counters.damage_received[yellow][capital],
                              17.0);
        TestRunner->TestEqual(
            TEXT("Kill uses committed team"), counters.kills[green][turret], uint64{1});
        TestRunner->TestEqual(
            TEXT("Loss uses committed team"), counters.losses[yellow][capital], uint64{1});
        TestRunner->TestEqual(TEXT("Destruction uses committed team"),
                              counters.destroyed[yellow][capital],
                              uint64{1});
        TestRunner->TestEqual(TEXT("Kill matrix uses committed teams"),
                              counters.kill_matrix[green][yellow],
                              uint64{1});
        registry_.end_tick();
        auto const replacement{ml::registry_tests::make_entities(1)};
        registry_.add_entities(view_of(replacement));
        TestRunner->TestTrue(TEXT("Reused slot preserves victim's last team"),
                             registry_.get_unique_entities().teams[1] ==
                                 ml::simulation::Team::Yellow);
    }

    TEST_METHOD(StaleKillersRetainCreditAcrossTicksAndSlotReuse)
    {
        auto data{ml::registry_tests::make_entities(3)};
        auto const handles{ml::registry_tests::make_handles(
            registry_.add_entities(view_of(data)).registry_handles)};
        data.alive[0] = 0;
        data.alive[1] = 0;
        EntityDeathInfo deaths;
        deaths.add(ml::simulation::DeathReason::Combat, handles[1], handles[0]);
        deaths.add(ml::simulation::DeathReason::Unknown, handles[0], {});
        registry_.queue_entity_updates({handles, view_of(data)}, deaths);
        registry_.commit_updates();
        auto const dead{registry_.get_dead_entities_this_frame()};
        TestRunner->TestEqual(TEXT("Both deaths recorded"), static_cast<int32>(dead.size()), 2);
        TestRunner->TestTrue(TEXT("Death order follows queue"),
                             dead[0] == handles[1] && dead[1] == handles[0]);
        TestRunner->TestEqual(
            TEXT("Only attributed death counts as kill"), registry_.count_kills(), 1);
        registry_.end_tick();
        auto const replacement{ml::registry_tests::make_entities(2, 40)};
        auto const replacements{ml::registry_tests::make_handles(
            registry_.add_entities(view_of(replacement)).registry_handles)};
        TestRunner->TestTrue(TEXT("Killer handle is stale"), registry_.is_stale(handles[0]));
        auto update{ml::registry_tests::make_entities(1)};
        update.alive[0] = 0;
        deaths.reset();
        deaths.add(ml::simulation::DeathReason::Combat, handles[2], handles[0]);
        registry_.queue_entity_updates({std::vector{handles[2]}, view_of(update)}, deaths);
        registry_.commit_updates();
        TestRunner->TestEqual(
            TEXT("Historical killer gains credit"), registry_.get_kills({0}), uint32{2});
        TestRunner->TestEqual(TEXT("Victim records historical killer"),
                              registry_.get_unique_entities().killed_by[2].id,
                              0);
        TestRunner->TestEqual(TEXT("Replacement gains no credit"),
                              registry_.get_kills(registry_.find_unique_id(replacements[1])),
                              uint32{0});
        TestRunner->TestEqual(TEXT("Kills accumulate across ticks"), registry_.count_kills(), 2);
        check_counts();
        registry_.end_tick();
        registry_.commit_updates();
        TestRunner->TestEqual(TEXT("Deaths cleared at end tick"),
                              static_cast<int32>(registry_.get_dead_entities_this_frame().size()),
                              0);
        TestRunner->TestEqual(TEXT("Cleared deaths are not replayed"), registry_.count_kills(), 2);
    }

    TEST_METHOD(RefreshDistinguishesNullStaleDeadAndLiveHandles)
    {
        auto data{ml::registry_tests::make_entities(3)};
        auto const handles{ml::registry_tests::make_handles(
            registry_.add_entities(view_of(data)).registry_handles)};
        TestRunner->TestTrue(TEXT("Default is null"),
                             registry_.analyse_handle({}) ==
                                 ml::simulation::RegistryHandleState::Null);
        TestRunner->TestTrue(TEXT("Out of range is invalid"),
                             registry_.analyse_handle({3, 0}) ==
                                 ml::simulation::RegistryHandleState::Invalid);
        TestRunner->TestTrue(TEXT("Future generation is invalid"),
                             registry_.analyse_handle({0, 1}) ==
                                 ml::simulation::RegistryHandleState::Invalid);
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
        std::vector<float> radii;
        radii.assign(4, -1.f);
        registry_.refresh_entity_data(
            refreshed, locations.get_view(), velocities.get_view(), radii);
        for (int32 index{}; index < 3; ++index) {
            TestRunner->TestTrue(TEXT("Null, stale and dead become null"),
                                 refreshed[index].is_null());
            TestRunner->TestEqual(TEXT("Cleared location"),
                                  ml::to_unreal(locations[index]),
                                  ml::to_unreal(ml::make_vector3f(0.f, 0.f, 0.f)));
            TestRunner->TestEqual(TEXT("Cleared velocity"),
                                  ml::to_unreal(velocities[index]),
                                  ml::to_unreal(ml::make_vector3f(0.f, 0.f, 0.f)));
            TestRunner->TestEqual(TEXT("Cleared radius"), radii[index], 0.f);
        }
        TestRunner->TestTrue(TEXT("Live handle retained"), refreshed[3] == handles[2]);
        TestRunner->TestEqual(TEXT("Live location refreshed"),
                              ml::to_unreal(locations[3]),
                              ml::to_unreal(data.locations[2]));
        TestRunner->TestEqual(TEXT("Live velocity refreshed"),
                              ml::to_unreal(velocities[3]),
                              ml::to_unreal(data.velocities[2]));
        TestRunner->TestEqual(TEXT("Live radius refreshed"), radii[3], data.radii[2]);
        registry_.refresh_entity_data(refreshed, {}, {}, radii);
        registry_.refresh_entity_data(refreshed, {}, velocities.get_view(), {});
        registry_.refresh_entity_data(refreshed, locations.get_view(), {}, {});
        registry_.refresh_entity_data({}, {}, {}, {});
        std::vector const location_handles{FRegistryEntityHandle{}, handles[1]};
        ml::simulation::Vectors3f dead_locations;
        dead_locations.add_defaulted(2);
        registry_.refresh_locations(location_handles, dead_locations.get_view());
        TestRunner->TestEqual(TEXT("Location-only refresh accepts current dead handle"),
                              ml::to_unreal(dead_locations[1]),
                              ml::to_unreal(data.locations[1]));
        registry_.end_tick();
    }

    TEST_METHOD(DamageQueuesPreserveBatchesAndResetStartsANewIdentityLifetime)
    {
        auto const data{ml::registry_tests::make_entities(2)};
        auto const handles{ml::registry_tests::make_handles(
            registry_.add_entities(view_of(data)).registry_handles)};
        DirectDamageEvents first;
        first.add(handles[1], 5, handles[0]);
        first.add(handles[0], 8, {});
        DirectDamageEvents second;
        second.add(handles[1], 13, handles[0]);
        registry_.queue_direct_damage_events(first);
        registry_.queue_direct_damage_events(second);
        first.damage_amounts[0] = 999;
        auto const& queued{registry_.get_direct_damage_queue_view()};
        TestRunner->TestEqual(TEXT("Damage batches append"), queued.num(), 3);
        TestRunner->TestTrue(
            TEXT("Damage order and values preserved"),
            std::ranges::equal(queued.damage_amounts, std::array<int32, 3>{5, 8, 13}));
        TestRunner->TestTrue(TEXT("Damage victims preserved"),
                             std::ranges::equal(queued.damaged_entities,
                                                std::array{handles[1], handles[0], handles[1]}));
        TestRunner->TestTrue(
            TEXT("Damage instigators preserved"),
            std::ranges::equal(queued.instigators,
                               std::array{handles[0], FRegistryEntityHandle{}, handles[0]}));
        registry_.commit_updates();
        TestRunner->TestEqual(TEXT("Commit retains damage queue"), queued.num(), 3);
        registry_.end_tick();
        TestRunner->TestEqual(TEXT("End tick clears damage queue"), queued.num(), 0);
        auto pending{data};
        pending.alive[1] = 0;
        EntityDeathInfo deaths;
        deaths.add(ml::simulation::DeathReason::Combat, handles[1], handles[0]);
        registry_.queue_entity_updates({handles, view_of(pending)}, deaths);
        registry_.commit_updates();
        registry_.queue_direct_damage_events(second);
        registry_.reset();
        TestRunner->TestEqual(TEXT("Reset clears slots"), registry_.get_num_elements(), 0);
        TestRunner->TestEqual(
            TEXT("Reset clears history"), registry_.get_num_unique_ids_issued(), 0);
        TestRunner->TestEqual(TEXT("Reset clears kills"), registry_.count_kills(), 0);
        TestRunner->TestEqual(TEXT("Reset clears damage"), queued.num(), 0);
        TestRunner->TestEqual(TEXT("Reset clears deaths"),
                              static_cast<int32>(registry_.get_dead_entities_this_frame().size()),
                              0);
        check_counts();
        auto const empty{registry_.add_entities(view_of(data, 0, 0))};
        TestRunner->TestEqual(
            TEXT("Empty spawn returns no handles"), empty.registry_handles.num(), 0);
        TestRunner->TestFalse(TEXT("Empty spawn has null first ID"), empty.first_id.is_valid());
        auto const spawned{registry_.add_entities(view_of(data))};
        TestRunner->TestEqual(TEXT("Reset restarts IDs"), spawned.first_id.id, 0);
        TestRunner->TestTrue(TEXT("Reset restarts generations"),
                             ml::registry_tests::make_handles(spawned.registry_handles) == handles);
        registry_.commit_updates();
        check_row(data, 1, handles[1]);
        TestRunner->TestEqual(
            TEXT("Reset discards pending death accounting"), registry_.count_kills(), 0);
        auto const& counters{registry_.get_combat_telemetry()};
        TestRunner->TestEqual(TEXT("Reset discards previous hits"),
                              counters.hits[static_cast<int32>(ml::simulation::Team::Red)]
                                           [static_cast<int32>(ml::simulation::EntityType::Turret)],
                              uint64{0});
        check_counts();
        registry_.end_tick();
    }
};
