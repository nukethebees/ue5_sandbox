#include "SandboxISMCComponent.h"

#include "Engine/StaticMesh.h"
#include "UObject/UObjectGlobals.h"

#include <CQTest.h>

TEST_CLASS(SandboxISMCComponent, "SandboxISMC.UnitTests")
{
    TEST_METHOD(BuildsCompleteSnapshotsInFixedChunks)
    {
        auto* component{NewObject<USandboxISMCComponent>()};
        auto* mesh{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};
        TestRunner->TestNotNull(TEXT("The engine cube mesh loads"), mesh);
        if (mesh == nullptr) {
            return;
        }

        component->set_static_mesh(*mesh);
        constexpr int32 instance_count{2050};
        TArray<FVector3f> positions;
        positions.SetNumUninitialized(instance_count);
        for (auto index = 0; index < instance_count; ++index) {
            positions[index] = {static_cast<float>(index), 0.0f, 0.0f};
        }

        TArray<int32> chunk_offsets;
        TArray<int32> chunk_counts;
        component->set_instances(instance_count,
                                 ESandboxISMCParallelism::Sequential,
                                 [&](FSandboxISMCInstanceChunkWriter& chunk) {
                                     chunk_offsets.Add(chunk.first_index());
                                     chunk_counts.Add(chunk.num());
                                     auto const first_index{chunk.first_index()};
                                     for (auto local_index = 0; local_index < chunk.num();
                                          ++local_index) {
                                         chunk.set_transform(local_index,
                                                             positions[first_index + local_index],
                                                             FQuat4f::Identity,
                                                             FVector3f::OneVector);
                                     }
                                 });

        TestRunner->TestEqual(TEXT("The component reports the submitted instance count"),
                              component->get_instance_count(),
                              instance_count);
        TestRunner->TestEqual(
            TEXT("The snapshot is split into three chunks"), chunk_offsets.Num(), 3);
        TestRunner->TestEqual(TEXT("The second chunk starts at 1024"), chunk_offsets[1], 1024);
        TestRunner->TestEqual(TEXT("The final chunk contains the remainder"), chunk_counts[2], 2);
        auto const metrics{component->get_update_metrics()};
        TestRunner->TestEqual(
            TEXT("Metrics report the snapshot size"), metrics.instance_count, instance_count);
        TestRunner->TestEqual(TEXT("Metrics report a complete packed upload"),
                              metrics.upload_bytes,
                              static_cast<uint64>(instance_count) *
                                  sizeof(FSandboxISMCRenderInstance));
        TestRunner->TestTrue(TEXT("Snapshot bounds are non-empty"),
                             component->CalcBounds(FTransform::Identity).SphereRadius > 0.0);
    }

    TEST_METHOD(ReplacesSnapshotsAndClearsImmediately)
    {
        auto* component{NewObject<USandboxISMCComponent>()};
        auto* cube{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};
        auto* sphere{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"))};
        TestRunner->TestNotNull(TEXT("The engine cube mesh loads"), cube);
        TestRunner->TestNotNull(TEXT("The engine sphere mesh loads"), sphere);
        if (cube == nullptr || sphere == nullptr) {
            return;
        }

        component->set_static_mesh(*cube);
        component->set_instances(
            4, ESandboxISMCParallelism::Sequential, [&](FSandboxISMCInstanceChunkWriter& chunk) {
                for (auto index = 0; index < chunk.num(); ++index) {
                    chunk.set_transform(index,
                                        {static_cast<float>(index), 0.0f, 0.0f},
                                        FQuat4f::Identity,
                                        FVector3f::OneVector);
                }
            });
        component->set_instances(
            1, ESandboxISMCParallelism::Sequential, [&](FSandboxISMCInstanceChunkWriter& chunk) {
                chunk.set_transform(
                    0, {100.0f, 0.0f, 0.0f}, FQuat4f::Identity, FVector3f::OneVector);
            });
        TestRunner->TestEqual(TEXT("The newest snapshot replaces the previous pending snapshot"),
                              component->get_instance_count(),
                              1);

        component->clear_instances();
        TestRunner->TestEqual(
            TEXT("Clear removes the current snapshot"), component->get_instance_count(), 0);
        TestRunner->TestEqual(TEXT("Clear resets bounds"),
                              component->CalcBounds(FTransform::Identity).SphereRadius,
                              0.0);

        auto callback_invoked{false};
        component->set_instances(
            0, ESandboxISMCParallelism::Parallel, [&](FSandboxISMCInstanceChunkWriter&) {
                callback_invoked = true;
            });
        TestRunner->TestFalse(TEXT("Empty snapshots do not invoke the callback"), callback_invoked);

        component->set_instances(
            1, ESandboxISMCParallelism::Sequential, [&](FSandboxISMCInstanceChunkWriter& chunk) {
                chunk.set_transform(
                    0, FVector3f::ZeroVector, FQuat4f::Identity, FVector3f::OneVector);
            });
        component->set_static_mesh(*sphere);
        TestRunner->TestEqual(
            TEXT("Changing the mesh clears the snapshot"), component->get_instance_count(), 0);
    }

    TEST_METHOD(AcceptsGenericSourcesAndMatchesParallelBounds)
    {
        auto* sequential{NewObject<USandboxISMCComponent>()};
        auto* parallel{NewObject<USandboxISMCComponent>()};
        auto* mesh{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};
        TestRunner->TestNotNull(TEXT("The engine cube mesh loads"), mesh);
        if (mesh == nullptr) {
            return;
        }
        sequential->set_static_mesh(*mesh);
        parallel->set_static_mesh(*mesh);

        constexpr int32 instance_count{4097};
        TArray<FVector3f> positions;
        TArray<FRotator3f> rotations;
        positions.SetNumUninitialized(instance_count);
        rotations.SetNumUninitialized(instance_count);
        for (auto index = 0; index < instance_count; ++index) {
            positions[index] = {static_cast<float>(index), static_cast<float>(index % 7), 0.0f};
            rotations[index] = {0.0f, static_cast<float>(index % 360), 0.0f};
        }

        auto const submit{[&](USandboxISMCComponent& target, ESandboxISMCParallelism parallelism) {
            target.set_instances(
                instance_count, parallelism, [&](FSandboxISMCInstanceChunkWriter& chunk) {
                    auto const first_index{chunk.first_index()};
                    for (auto local_index = 0; local_index < chunk.num(); ++local_index) {
                        auto const source_index{first_index + local_index};
                        chunk.set_transform(local_index,
                                            positions[source_index],
                                            rotations[source_index].Quaternion(),
                                            FVector3f::OneVector);
                    }
                });
        }};
        submit(*sequential, ESandboxISMCParallelism::Sequential);
        submit(*parallel, ESandboxISMCParallelism::Parallel);

        auto const sequential_bounds{sequential->CalcBounds(FTransform::Identity)};
        auto const parallel_bounds{parallel->CalcBounds(FTransform::Identity)};
        TestRunner->TestTrue(TEXT("Parallel snapshots produce the same origin"),
                             sequential_bounds.Origin.Equals(parallel_bounds.Origin));
        TestRunner->TestTrue(TEXT("Parallel snapshots produce the same extent"),
                             sequential_bounds.BoxExtent.Equals(parallel_bounds.BoxExtent));
        TestRunner->TestEqual(TEXT("Parallel snapshots retain every source row"),
                              parallel->get_instance_count(),
                              instance_count);
    }

    TEST_METHOD(SupportsChunkAndParallelismBoundaries)
    {
        auto* component{NewObject<USandboxISMCComponent>()};
        TArray<int32> const counts{0, 1, 1023, 1024, 1025, 4095, 4096, 4097};
        TArray<ESandboxISMCParallelism> const policies{
            ESandboxISMCParallelism::Sequential,
            ESandboxISMCParallelism::Parallel,
            ESandboxISMCParallelism::Auto,
        };

        for (auto const policy : policies) {
            for (auto const count : counts) {
                TArray<uint8> visited;
                visited.SetNumZeroed(count);
                component->set_instances(
                    count, policy, [&](FSandboxISMCInstanceChunkWriter& chunk) {
                        auto const first_index{chunk.first_index()};
                        for (auto local_index = 0; local_index < chunk.num(); ++local_index) {
                            auto const source_index{first_index + local_index};
                            ++visited[source_index];
                            chunk.set_transform(local_index,
                                                {static_cast<float>(source_index), 0.0f, 0.0f},
                                                FQuat4f::Identity,
                                                FVector3f::OneVector);
                        }
                    });

                auto all_visited{true};
                for (auto const value : visited) {
                    all_visited &= value == 1;
                }
                TestRunner->TestTrue(TEXT("Every source row is visited exactly once"), all_visited);
                TestRunner->TestEqual(TEXT("The complete boundary snapshot is retained"),
                                      component->get_instance_count(),
                                      count);
            }
        }
    }
};
