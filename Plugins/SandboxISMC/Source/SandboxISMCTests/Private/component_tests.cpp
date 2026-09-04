#include "SandboxISMCComponent.h"

#include "Engine/StaticMesh.h"
#include "UObject/UObjectGlobals.h"

#include <CQTest.h>

TEST_CLASS(SandboxISMCComponent, "SandboxISMC.UnitTests")
{
    TEST_METHOD(DelegatesStoragePreparationMetricsAndBounds)
    {
        auto* component{NewObject<USandboxISMCComponent>()};
        auto* mesh{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};
        TestRunner->TestNotNull(TEXT("The engine cube mesh loads"), mesh);
        if (mesh == nullptr) {
            return;
        }

        TArray<FVector3f> const positions{{0.0f, 0.0f, 0.0f}, {200.0f, 0.0f, 0.0f}};
        TestRunner->TestEqual(TEXT("The component delegates batch insertion"),
                              component->add_instances(positions),
                              0);
        component->set_static_mesh(*mesh);

        TestRunner->TestEqual(
            TEXT("The component reports state instance count"), component->get_instance_count(), 2);
        auto const metrics{component->get_update_metrics()};
        TestRunner->TestEqual(
            TEXT("Metrics report committed instances"), metrics.instance_count, 2);
        TestRunner->TestEqual(
            TEXT("The initial commit packs both instances"), metrics.dirty_instance_count, 2);
        TestRunner->TestTrue(TEXT("Committed mesh bounds are non-empty"),
                             component->CalcBounds(FTransform::Identity).SphereRadius > 0.0);

        component->clear_instances();
        component->commit_instance_updates();
        TestRunner->TestEqual(
            TEXT("Clear delegates to state storage"), component->get_instance_count(), 0);
        TestRunner->TestEqual(TEXT("Clear commits empty bounds"),
                              component->CalcBounds(FTransform::Identity).SphereRadius,
                              0.0);
    }
};
