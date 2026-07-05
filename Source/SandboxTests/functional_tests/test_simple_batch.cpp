#include "test_simple_batch.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestStaticTurrets.h>

#include <SandboxCore/soa_vector_utils.h>

ATestSimpleBatch::ATestSimpleBatch() {
    PrimaryActorTick.TickGroup = ETickingGroup::TG_PostUpdateWork;
}

void ATestSimpleBatch::BeginPlay() {
    Super::BeginPlay();
    StartTest();
}
void ATestSimpleBatch::PrepareTest() {
    Super::PrepareTest();

    pre_check();
}
void ATestSimpleBatch::StartTest() {
    Super::StartTest();

    pre_check();

    all_passed = true;

    update_kills();

    auto const unique_ids{entity_registry->get_active_unique_ids()};
    all_passed &= AssertFalse(unique_ids.IsEmpty(), TEXT("No unique ids"));

    if (!all_passed) { handle_fail(); }
}
void ATestSimpleBatch::Tick(float dt) {
    Super::Tick(dt);

    if (!IsRunning()) { return; }

    update_kills();
    check_alive_matches_kills();

    if (kills == expected_kills) { handle_kill_count_reached(); }

    all_passed &= AssertTrue(kills <= expected_kills, TEXT("Too many kills"));

    if (!all_passed) { handle_fail(); }
}
void ATestSimpleBatch::OnTimeout() {
    Super::OnTimeout();
}
void ATestSimpleBatch::FinishTest(EFunctionalTestResult TestResult, FString const& Message) {
    switch (TestResult) {
        case EFunctionalTestResult::Failed: {
            LogMessage(get_fail_message());
        }
        case EFunctionalTestResult::Succeeded: {
            LogMessage(get_fail_message());
        }
        default: {
            break;
        }
    }

    Super::FinishTest(TestResult, Message);
}

void ATestSimpleBatch::pre_check() {
    if (!IsValid(entity_registry)) {
        FinishTest(EFunctionalTestResult::Failed, TEXT("Entity registry was not assigned"));
    }

    if (!IsValid(turrets)) {
        FinishTest(EFunctionalTestResult::Failed, TEXT("turrets was not assigned"));
    }
}

void ATestSimpleBatch::check_alive_matches_kills() {
    auto const n_alive{entity_registry->count_alive()};
    auto const n_unique{entity_registry->get_num_unique_ids_issued()};

    auto const n_alive_expected{n_unique - kills};

    all_passed &= AssertEqual_Int(n_alive, n_alive_expected, TEXT("number of alive entities"));
}
void ATestSimpleBatch::update_kills() {
    kills = entity_registry->count_kills();
}

auto ATestSimpleBatch::get_fail_message() const -> FString {
    FString msg{};
    msg += TEXT(R"(#
# >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Test Fail
# <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
)");

    auto const& unique_entities{entity_registry->get_unique_entities()};
    auto const n{unique_entities.num()};

    msg += TEXT("\nTest data:");
    msg += FString::Printf(TEXT("\n    Expected kills: %d"), expected_kills);
    msg += FString::Printf(TEXT("\n    Kills: %d"), kills);

    msg += FString::Printf(TEXT("\nUnique entities: %d"), n);

    msg += TEXT("\nActive entity data:");
    auto const n_active_alive{entity_registry->get_num_alive_active_entities()};
    msg += FString::Printf(TEXT("\nAlive: %d"), n_active_alive);

    auto const n_active{entity_registry->get_num_elements()};

    auto const& entity_data{entity_registry->get_entity_data()};
    auto const generations{entity_registry->get_generations()};
    auto const active_ids{entity_registry->get_active_unique_ids()};

    for (int32 i{0}; i < n_active; ++i) {
        msg += FString::Printf(TEXT("\nIndex: %d, Gen: %d, Id: %d, Health: %d, Alive: %d, loc=%s"),
                               i,
                               generations[i],
                               active_ids[i].id,
                               entity_data.healths[i],
                               entity_data.alive[i],
                               *ml::get_vector3f(entity_data.locations, 1).ToCompactString());
    }

    msg += TEXT("\n\nGlobal id data:");
    msg += FString::Printf(TEXT("\nAlive: %d"), entity_registry->count_alive());
    msg += FString::Printf(TEXT("\nKills: %d"), kills);
    for (int32 i{0}; i < n; ++i) {
        msg += FString::Printf(TEXT("\nId: %d, Kills: %d, Alive: %d"),
                               i,
                               unique_entities.kills[i],
                               unique_entities.alive[i]);
    }

    auto const target_handles{turrets->get_target_handles()};
    msg += TEXT("\n\nTurret info");
    msg += TEXT("\nTarget handles:");
    for (auto const& handle : target_handles) {
        msg += FString::Printf(TEXT("\n    %s"), *handle.to_string());
    }

    return msg;
}
void ATestSimpleBatch::handle_fail() {
    FinishTest(EFunctionalTestResult::Failed, TEXT("Got fail"));
}

void ATestSimpleBatch::handle_kill_count_reached() {
    auto const target_handles{turrets->get_target_handles()};

    for (auto const& handle : target_handles) {
        if (entity_registry->is_valid_handle(handle)) {
            all_passed &= AssertFalse(entity_registry->get_alive(handle),
                                      TEXT("the entity should be regarded as dead"));
        }
    }

    if (!all_passed) { handle_fail(); }

    FinishTest(EFunctionalTestResult::Succeeded, TEXT("Passed"));
}
