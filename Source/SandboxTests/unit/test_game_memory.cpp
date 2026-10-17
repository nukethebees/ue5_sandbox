#include <ioj/sim/memory/game_memory.h>
#include <native/memory/retained_backing_resource.h>
#include <SpaceGameSimulation/memory/GameMemoryBootstrap.h>

#include <CQTest.h>

TEST_CLASS(GameMemoryBootstrap, "Sandbox.UnitTests")
{
    TEST_METHOD(OccupiedBackingFallsBackToLocalMemory)
    {
        ioj::memory::RetainedBackingResource upstream{1024};
        ioj::sim::GameMemory first{{.root_capacity_bytes = 256}, upstream};
        auto& delegate{FGameMemoryBootstrap::upstream_resource_delegate()};
        auto previous_delegate{delegate};
        delegate.BindLambda([&upstream](std::size_t bytes) -> std::pmr::memory_resource* {
            return upstream.can_allocate(bytes, ioj::memory::Backing::alignment) ? &upstream
                                                                                 : nullptr;
        });
        auto local_fallback{FGameMemoryBootstrap::create_game_memory({.root_capacity_bytes = 256})};
        TestRunner->TestTrue(
            TEXT("Bootstrap falls back locally while the editor-style backing is occupied"),
            local_fallback->backing_data() != upstream.data());
        delegate = previous_delegate;
    }
};
