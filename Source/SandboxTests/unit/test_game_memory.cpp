#include <SpaceGameSimulation/memory/GameMemory.h>
#include <SpaceGameSimulation/memory/GameMemoryBootstrap.h>

#include <Misc/AutomationTest.h>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameMemoryTest,
                                 "Sandbox.UnitTests.GameMemory",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

auto FGameMemoryTest::RunTest(FString const&) -> bool {
    FGameMemory memory{{.root_capacity_bytes = 512}};
    TestTrue(TEXT("Direct game memory owns its backing locally"), memory.owns_backing_locally());

    std::byte* first_address{};
    {
        auto block{memory.acquire_block(128, 64)};
        first_address = block.data();
        TestEqual(TEXT("A live block contributes its requested bytes"),
                  memory.get_stats().live_block_bytes,
                  SIZE_T{128});
    }
    auto reused{memory.acquire_block(128, 64)};
    TestEqual(TEXT("A returned exact block is reused"), reused.data(), first_address);

    auto exhaustion{memory.try_acquire_block(1024, 32)};
    TestFalse(TEXT("Root exhaustion is nonfatal through the try API"), exhaustion.has_value());
    auto const failure{memory.get_last_allocation_failure()};
    TestTrue(TEXT("Root exhaustion records a diagnostic"), failure.has_value());
    TestEqual(TEXT("Diagnostic records requested bytes"), failure->requested_bytes, SIZE_T{1024});
    TestEqual(TEXT("Diagnostic records alignment"), failure->alignment, SIZE_T{32});
    TestEqual(
        TEXT("Diagnostic records bytes already claimed"), failure->claimed_bytes, SIZE_T{128});
    TestEqual(
        TEXT("Diagnostic records total capacity"), failure->total_capacity_bytes, SIZE_T{512});

    auto backing{FGameMemoryBacking::create(1024)};
    auto first_lease{backing->try_acquire_lease()};
    TestTrue(TEXT("The first backing lease succeeds"), first_lease.IsSet());
    TestFalse(TEXT("A simultaneous backing lease fails"), backing->try_acquire_lease().IsSet());
    auto* const backing_address{first_lease->get().data()};

    auto moved_lease{MoveTemp(first_lease.GetValue())};
    TestFalse(TEXT("A moved-from lease is empty"), static_cast<bool>(first_lease.GetValue()));
    first_lease.Reset();
    TestTrue(TEXT("The moved lease keeps the backing occupied"), backing->is_leased());
    moved_lease = {};

    auto second_lease{backing->try_acquire_lease()};
    TestTrue(TEXT("A sequential backing lease succeeds"), second_lease.IsSet());
    TestEqual(TEXT("Sequential leases expose the persistent address"),
              second_lease->get().data(),
              backing_address);

    auto& delegate{FGameMemoryBootstrap::acquire_backing_delegate()};
    delegate.BindLambda([backing = backing.Get()] { return backing->try_acquire_lease(); });
    auto local_fallback{FGameMemoryBootstrap::create_game_memory({.root_capacity_bytes = 256})};
    TestTrue(TEXT("Bootstrap falls back locally while the editor-style backing is occupied"),
             local_fallback->owns_backing_locally());
    delegate.Unbind();

    second_lease.Reset();
    {
        auto external_lease{backing->try_acquire_lease()};
        TestTrue(TEXT("A lease for external game memory succeeds"), external_lease.IsSet());
        FGameMemory external_memory{MoveTemp(external_lease.GetValue()),
                                    {.root_capacity_bytes = 256}};
        TestFalse(TEXT("External game memory does not own its backing locally"),
                  external_memory.owns_backing_locally());
        TestEqual(TEXT("External game memory uses the leased backing"),
                  external_memory.backing_address(),
                  backing_address);
    }
    TestFalse(TEXT("Destroying external game memory releases its lease"), backing->is_leased());

    return true;
}
