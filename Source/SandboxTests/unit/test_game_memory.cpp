#include <CQTest.h>
#include <sandbox/simulation/memory/GameMemory.h>
#include <SpaceGameSimulation/memory/GameMemoryBootstrap.h>

TEST_CLASS(GameMemoryBootstrap, "Sandbox.UnitTests")
{
    TEST_METHOD(OccupiedBackingFallsBackToLocalMemory)
    {
        auto backing{FGameMemoryBacking::create(1024)};
        auto lease{backing->try_acquire_lease()};
        ASSERT_THAT(IsTrue(lease.has_value()));
        auto& delegate{FGameMemoryBootstrap::acquire_backing_delegate()};
        delegate.BindLambda([backing = backing.get()] { return backing->try_acquire_lease(); });
        auto local_fallback{FGameMemoryBootstrap::create_game_memory({.root_capacity_bytes = 256})};
        TestRunner->TestTrue(
            TEXT("Bootstrap falls back locally while the editor-style backing is occupied"),
            local_fallback->owns_backing_locally());
        delegate.Unbind();
    }
};
