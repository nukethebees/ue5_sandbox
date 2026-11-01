#include <native/memory/bump_resource.h>
#include <SpaceGameSimulation/memory/GameMemoryBootstrap.h>

#include <CQTest.h>

TEST_CLASS(GameMemoryBootstrap, "Sandbox.UnitTests")
{
    TEST_METHOD(UsesProvidedBumpResource)
    {
        ioj::memory::BumpResource upstream{1024};
        auto& delegate{FGameMemoryBootstrap::upstream_resource_delegate()};
        auto previous_delegate{delegate};
        delegate.BindLambda([&upstream]() -> ioj::memory::BumpResource& { return upstream; });
        TestRunner->TestTrue(TEXT("Bootstrap provides the retained resource"),
                             &delegate.Execute() == &upstream);
        delegate = previous_delegate;
    }
};
