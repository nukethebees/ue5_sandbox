#include "Sandbox/batch_game/TestCapitalShipFighterSpawnQueueSoA.h"

#include "SandboxCore/soa_permutation.h"
#include "SandboxCore/tick_countdown.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGeneratedSoASortTest,
                                 "Sandbox.soa.generated_sort",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

auto FGeneratedSoASortTest::RunTest(FString const&) -> bool {
    using Queue = TestCapitalShipFighterSpawnQueue;

    auto add_row = [](Queue& queue, int32 const id, ETestTeam const team) {
        queue.locations.add(
            static_cast<float>(id), static_cast<float>(id + 100), static_cast<float>(id + 200));
        queue.rotations.pitches.Add(static_cast<float>(id + 300));
        queue.rotations.yaws.Add(static_cast<float>(id + 400));
        queue.rotations.rolls.Add(static_cast<float>(id + 500));
        queue.teams.Add(team);
        queue.targets.Add(FRegistryEntityHandle{id, id + 1000});
    };
    auto verify_row_associations = [this](Queue const& queue) {
        auto const n{queue.num()};
        for (int32 i{}; i < n; ++i) {
            auto const id{queue.targets[i].index};
            TestEqual(
                TEXT("Target generation remains paired"), queue.targets[i].generation, id + 1000);
            TestEqual(TEXT("Nested location x remains paired"),
                      queue.locations.xs[i],
                      static_cast<float>(id));
            TestEqual(TEXT("Nested location y remains paired"),
                      queue.locations.ys[i],
                      static_cast<float>(id + 100));
            TestEqual(TEXT("Nested location z remains paired"),
                      queue.locations.zs[i],
                      static_cast<float>(id + 200));
            TestEqual(TEXT("Nested rotation pitch remains paired"),
                      queue.rotations.pitches[i],
                      static_cast<float>(id + 300));
            TestEqual(TEXT("Nested rotation yaw remains paired"),
                      queue.rotations.yaws[i],
                      static_cast<float>(id + 400));
            TestEqual(TEXT("Nested rotation roll remains paired"),
                      queue.rotations.rolls[i],
                      static_cast<float>(id + 500));
        }
    };
    auto by_team = [](auto const& queue, int32 const lhs, int32 const rhs) {
        return queue.teams[lhs] < queue.teams[rhs];
    };
    auto by_team_then_descending_target = [](auto const& queue, int32 const lhs, int32 const rhs) {
        if (queue.teams[lhs] != queue.teams[rhs]) {
            return queue.teams[lhs] < queue.teams[rhs];
        }
        return queue.targets[lhs].index > queue.targets[rhs].index;
    };

    TArray<int32> scratch_indices;
    scratch_indices.SetNumUninitialized(6);
    scratch_indices[4] = 1234;
    scratch_indices[5] = 5678;

    Queue duplicate_teams;
    add_row(duplicate_teams, 30, ETestTeam::Blue);
    add_row(duplicate_teams, 10, ETestTeam::Red);
    add_row(duplicate_teams, 20, ETestTeam::Red);
    add_row(duplicate_teams, 40, ETestTeam::Green);
    duplicate_teams.sort(by_team, scratch_indices);
    verify_row_associations(duplicate_teams);
    for (int32 i{1}; i < duplicate_teams.num(); ++i) {
        TestTrue(TEXT("Single-field sort orders keys"),
                 duplicate_teams.teams[i - 1] <= duplicate_teams.teams[i]);
    }
    TestEqual(TEXT("Unused scratch index is unchanged"), scratch_indices[4], 1234);
    TestEqual(TEXT("Second unused scratch index is unchanged"), scratch_indices[5], 5678);

    duplicate_teams.sort(by_team_then_descending_target, scratch_indices);
    verify_row_associations(duplicate_teams);
    for (int32 i{1}; i < duplicate_teams.num(); ++i) {
        auto const previous_team{duplicate_teams.teams[i - 1]};
        auto const current_team{duplicate_teams.teams[i]};
        TestTrue(TEXT("Multi-field sort orders first key"), previous_team <= current_team);
        if (previous_team == current_team) {
            TestTrue(TEXT("Multi-field sort orders second key"),
                     duplicate_teams.targets[i - 1].index > duplicate_teams.targets[i].index);
        }
    }

    Queue already_sorted;
    add_row(already_sorted, 10, ETestTeam::Red);
    add_row(already_sorted, 20, ETestTeam::Green);
    add_row(already_sorted, 30, ETestTeam::Blue);
    already_sorted.sort(
        [](auto const& queue, int32 const lhs, int32 const rhs) {
            return queue.targets[lhs].index < queue.targets[rhs].index;
        },
        scratch_indices);
    verify_row_associations(already_sorted);
    TestEqual(TEXT("Already-sorted first row remains first"), already_sorted.targets[0].index, 10);
    TestEqual(TEXT("Already-sorted last row remains last"), already_sorted.targets[2].index, 30);

    Queue reverse_sorted;
    add_row(reverse_sorted, 30, ETestTeam::Blue);
    add_row(reverse_sorted, 20, ETestTeam::Green);
    add_row(reverse_sorted, 10, ETestTeam::Red);
    reverse_sorted.sort(
        [](auto const& queue, int32 const lhs, int32 const rhs) {
            return queue.targets[lhs].index < queue.targets[rhs].index;
        },
        scratch_indices);
    verify_row_associations(reverse_sorted);
    TestEqual(TEXT("Reverse-sorted first row is reordered"), reverse_sorted.targets[0].index, 10);
    TestEqual(TEXT("Reverse-sorted middle row is reordered"), reverse_sorted.targets[1].index, 20);
    TestEqual(TEXT("Reverse-sorted last row is reordered"), reverse_sorted.targets[2].index, 30);

    Queue empty;
    TArray<int32> empty_scratch;
    empty.sort(by_team, empty_scratch);
    TestEqual(TEXT("Empty SOA remains empty"), empty.num(), 0);

    Queue single;
    add_row(single, 42, ETestTeam::Orange);
    TArray<int32> single_scratch;
    single_scratch.SetNumUninitialized(2);
    single_scratch[1] = 99;
    single.sort(by_team, single_scratch);
    verify_row_associations(single);
    TestEqual(TEXT("Single row remains present"), single.targets[0].index, 42);
    TestEqual(TEXT("Single-row unused scratch remains unchanged"), single_scratch[1], 99);

    FTickCountdown8 countdown{3, 0};
    countdown.set_counter(0, 1);
    countdown.set_counter(1, 2);
    countdown.set_counter(2, 3);
    TArray<int32> countdown_indices{2, 0, 1};
    ml::apply_permutation(countdown, countdown_indices);
    TestEqual(TEXT("Countdown first counter follows permutation"), countdown.counters()[0], 3);
    TestEqual(TEXT("Countdown second counter follows permutation"), countdown.counters()[1], 1);
    TestEqual(TEXT("Countdown third counter follows permutation"), countdown.counters()[2], 2);
    TestEqual(TEXT("Countdown permutation indices are restored"), countdown_indices[0], 2);

    return true;
}
