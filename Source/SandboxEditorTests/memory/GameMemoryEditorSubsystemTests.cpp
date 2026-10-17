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

    auto* const upstream{subsystem->upstream_resource(1024)};
    TestNotNull(TEXT("The first editor backing allocation is available"), upstream);
    if (upstream == nullptr) {
        return false;
    }
    auto* const persistent_data{subsystem->backing_data()};
    {
        ioj::sim::GameMemory first{{.root_capacity_bytes = 1024}, *upstream};
        TestEqual(TEXT("Game memory uses the subsystem's persistent backing"),
                  first.backing_data(),
                  persistent_data);
        TestNull(TEXT("A simultaneous editor allocation is unavailable"),
                 subsystem->upstream_resource(1024));
        auto fallback{FGameMemoryBootstrap::create_game_memory({.root_capacity_bytes = 1024})};
        TestTrue(TEXT("Bootstrap uses local memory while the editor backing is occupied"),
                 fallback->backing_data() != persistent_data);
    }
    auto second{FGameMemoryBootstrap::create_game_memory({.root_capacity_bytes = 1024})};
    TestEqual(TEXT("Sequential game-memory instances reuse the backing address"),
              second->backing_data(),
              persistent_data);
    return true;
}
