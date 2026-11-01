#include <SpaceGameSimulation/memory/GameMemoryBootstrap.h>

#include <Editor.h>
#include <memory/GameMemoryEditorSubsystem.h>
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

    auto& delegate{FGameMemoryBootstrap::upstream_resource_delegate()};
    TestTrue(TEXT("The editor subsystem owns the bootstrap binding"),
             delegate.IsBoundToObject(subsystem));

    auto& upstream{subsystem->upstream_resource()};
    TestTrue(TEXT("Bootstrap exposes the editor resource"), &delegate.Execute() == &upstream);
    auto* const first{upstream.allocate(1024, 64)};
    upstream.deallocate(first, 1024, 64);
    upstream.reset();
    auto* const second{upstream.allocate(1024, 64)};
    TestEqual(TEXT("Reset reuses the retained allocation"), second, first);
    upstream.deallocate(second, 1024, 64);
    upstream.reset();
    return true;
}
