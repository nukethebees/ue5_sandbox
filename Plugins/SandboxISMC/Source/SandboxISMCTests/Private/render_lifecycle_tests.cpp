#include "SandboxISMCComponent.h"

#include "Components/MapTestSpawner.h"

#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RenderingThread.h"
#include "UObject/UObjectGlobals.h"

#include <CQTest.h>

TEST_CLASS(SandboxISMCRenderLifecycle, "SandboxISMC.RenderTests")
{
    TUniquePtr<FMapTestSpawner> spawner{nullptr};

    BEFORE_EACH()
    {
        spawner = FMapTestSpawner::CreateFromTempLevel(TestCommandBuilder);
        spawner->AddWaitUntilLoadedCommand(TestRunner);
    }

    AFTER_EACH()
    { spawner.Reset(); }

    TEST_METHOD(PreservesTheLatestSnapshotAcrossRenderProxyRecreation)
    {
        TestCommandBuilder.Do([this] {
            auto& world{spawner->GetWorld()};
            auto* mesh{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};
            TestRunner->TestNotNull(TEXT("The engine cube mesh loads"), mesh);
            if (mesh == nullptr) {
                return;
            }

            auto* const actor{world.SpawnActor<AActor>()};
            auto* const root{NewObject<USceneComponent>(actor, TEXT("Root"))};
            actor->SetRootComponent(root);
            actor->AddInstanceComponent(root);
            root->RegisterComponent();

            auto* const component{NewObject<USandboxISMCComponent>(actor, TEXT("SandboxISMC"))};
            actor->AddInstanceComponent(component);
            component->SetupAttachment(root);
            component->set_static_mesh(*mesh);
            component->set_num_custom_data_floats(3);

            auto const submit{[&](int32 const instance_count, float const value) {
                component->set_instances(
                    instance_count,
                    ESandboxISMCParallelism::Auto,
                    [=](FSandboxISMCInstanceChunkWriter& chunk) {
                        auto const [first_index, chunk_count]{chunk.range()};
                        for (auto local_index = 0; local_index < chunk_count; ++local_index) {
                            auto const instance_index{first_index + local_index};
                            chunk.set_transform(local_index,
                                                {static_cast<float>(instance_index), value, 0.0f},
                                                FQuat4f::Identity,
                                                FVector3f::OneVector);
                            auto custom_data{chunk.custom_data(local_index)};
                            custom_data[0] = value;
                            custom_data[1] = static_cast<float>(instance_index);
                            custom_data[2] = 1.0f;
                        }
                    });
            }};

            submit(4, 0.0f);
            component->RegisterComponent();
            world.SendAllEndOfFrameUpdates();
            FlushRenderingCommands();

            TestRunner->TestTrue(TEXT("The component creates render state"),
                                 component->IsRenderStateCreated());
            TestRunner->TestNotNull(TEXT("The first snapshot creates a scene proxy"),
                                    component->GetSceneProxy());

            TArray<int32> const update_counts{1, 1024, 1025, 4096, 4097, 33};
            auto const update_count{update_counts.Num()};
            for (auto update_index = 0; update_index < update_count; ++update_index) {
                submit(update_counts[update_index], static_cast<float>(update_index + 1));
                world.SendAllEndOfFrameUpdates();
            }
            FlushRenderingCommands();

            auto const final_instance_count{update_counts.Last()};
            TestRunner->TestEqual(TEXT("Rapid updates retain the final snapshot"),
                                  component->get_instance_count(),
                                  final_instance_count);
            TestRunner->TestNotNull(TEXT("Rapid updates retain the scene proxy"),
                                    component->GetSceneProxy());

            component->MarkRenderStateDirty();
            world.SendAllEndOfFrameUpdates();
            FlushRenderingCommands();

            TestRunner->TestTrue(TEXT("Proxy recreation restores render state"),
                                 component->IsRenderStateCreated());
            TestRunner->TestNotNull(TEXT("Proxy recreation reuses the committed snapshot"),
                                    component->GetSceneProxy());
            TestRunner->TestEqual(TEXT("Proxy recreation preserves the instance count"),
                                  component->get_instance_count(),
                                  final_instance_count);
            TestRunner->TestTrue(TEXT("Proxy recreation preserves snapshot bounds"),
                                 component->CalcBounds(FTransform::Identity).SphereRadius > 0.0);
        });
    }
};
