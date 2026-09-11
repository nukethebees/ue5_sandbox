#include <memory/GameMemoryEditorSubsystem.h>

#include <SpaceGameSimulation/memory/GameMemoryBootstrap.h>

#include <Editor.h>
#include <Misc/AutomationTest.h>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameMemoryEditorSubsystemTest,
                                 "Sandbox.EditorTests.GameMemorySubsystem",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

auto FGameMemoryEditorSubsystemTest::RunTest(FString const&) -> bool {
    auto* const subsystem{GEditor->GetEditorSubsystem<USandboxEditorGameMemorySubsystem>()};
    TestNotNull(TEXT("The editor game-memory subsystem is available"), subsystem);
    if (subsystem == nullptr) {
        return false;
    }

    auto& delegate{FGameMemoryBootstrap::acquire_backing_delegate()};
    TestTrue(TEXT("The editor subsystem owns the bootstrap binding"),
             delegate.IsBoundToObject(subsystem));

    auto first_lease{subsystem->acquire_backing()};
    TestTrue(TEXT("The first editor lease succeeds"), first_lease.IsSet());
    auto* const persistent_address{first_lease->get().data()};
    TestEqual(TEXT("The lease uses the subsystem's persistent backing"),
              persistent_address,
              subsystem->backing_address());
    TestFalse(TEXT("A simultaneous editor lease fails"), subsystem->acquire_backing().IsSet());

    auto fallback{FGameMemoryBootstrap::create_game_memory({.root_capacity_bytes = 1024})};
    TestTrue(TEXT("Bootstrap uses local memory while the editor backing is occupied"),
             fallback->owns_backing_locally());

    first_lease.Reset();
    auto second_lease{subsystem->acquire_backing()};
    TestTrue(TEXT("A sequential editor lease succeeds"), second_lease.IsSet());
    TestEqual(TEXT("Sequential editor leases reuse the backing address"),
              second_lease->get().data(),
              persistent_address);
    return true;
}
