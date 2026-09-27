#include "SandboxISMCComponent.h"

#include "Engine/StaticMesh.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include <CQTest.h>

TEST_CLASS(SandboxISMCComponent, "SandboxISMC.UnitTests")
{
    TEST_METHOD(RefreshesPropertyAssignedAndReplacementMeshBounds)
    {
        auto* component{NewObject<USandboxISMCComponent>()};
        auto* cube{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};
        auto* cone{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"))};
        auto* property{FindFProperty<FObjectProperty>(USandboxISMCComponent::StaticClass(),
                                                      TEXT("static_mesh_"))};
        if (!TestRunner->TestNotNull(TEXT("Cube loads"), cube) ||
            !TestRunner->TestNotNull(TEXT("Cone loads"), cone) ||
            !TestRunner->TestNotNull(TEXT("Mesh property exists"), property)) {
            return;
        }
        auto const verify_bounds{[&](UStaticMesh& mesh) {
            component->set_instances(
                1,
                FBox3f{FVector3f{-1000, -1000, -1000}, FVector3f{5000, 1000, 1000}},
                ESandboxISMCParallelism::Sequential,
                [](FSandboxISMCInstanceChunkWriter& chunk) {
                    {
                        FVector3f const transform_positions[]{FVector3f::ZeroVector};
                        FQuat4f const transform_rotations[]{FQuat4f::Identity};
                        FVector3f const transform_scales[]{FVector3f::OneVector};
                        chunk.set_transforms<ESandboxISMCBoundsMode::Calculate>(
                            transform_positions, transform_rotations, transform_scales);
                    }
                });
            auto const bounds{component->CalcBounds(FTransform::Identity)};
            TestRunner->TestTrue(TEXT("Bounds use the configured mesh origin"),
                                 bounds.Origin.Equals(mesh.GetBounds().Origin, 0.25));
            TestRunner->TestTrue(TEXT("Bounds use the configured mesh box"),
                                 bounds.GetBox().IsInsideOrOn(mesh.GetBounds().GetBox().Min) &&
                                     bounds.GetBox().IsInsideOrOn(mesh.GetBounds().GetBox().Max));
        }};
        property->SetObjectPropertyValue_InContainer(component, cube);
        component->set_static_mesh(*cube);
        verify_bounds(*cube);
        component->set_static_mesh(*cube);
        TestRunner->TestEqual(
            TEXT("An unchanged mesh preserves the snapshot"), component->get_instance_count(), 1);
        component->set_static_mesh(*cone);
        TestRunner->TestEqual(
            TEXT("Replacement invalidates the old snapshot"), component->get_instance_count(), 0);
        verify_bounds(*cone);

        auto* loaded_component{NewObject<USandboxISMCComponent>()};
        property->SetObjectPropertyValue_InContainer(loaded_component, cube);
        component = loaded_component;
        verify_bounds(*cube);
        component->clear_static_mesh();
        TestRunner->TestEqual(TEXT("Clearing the mesh invalidates bounds"),
                              component->CalcBounds(FTransform::Identity).SphereRadius,
                              0.0);
    }

    TEST_METHOD(ReservesWithoutSubmittingInstances)
    {
        auto* component{NewObject<USandboxISMCComponent>()};
        auto* mesh{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};
        if (!TestRunner->TestNotNull(TEXT("The engine cube mesh loads"), mesh)) {
            return;
        }
        component->set_static_mesh(*mesh);

        component->set_num_custom_data_floats(3);
        component->reserve_instances(4097);
        TestRunner->TestEqual(
            TEXT("Reserve does not publish instances"), component->get_instance_count(), 0);
        auto const submit{[&](int32 count) {
            component->set_instances(
                count,
                FBox3f{FVector3f{-1000, -1000, -1000}, FVector3f{5000, 1000, 1000}},
                ESandboxISMCParallelism::Sequential,
                [](FSandboxISMCInstanceChunkWriter& chunk) {
                    auto const count{chunk.num()};
                    TArray<FVector3f, TInlineAllocator<1024>> transform_positions;
                    transform_positions.SetNumUninitialized(chunk.num());
                    TArray<FQuat4f, TInlineAllocator<1024>> transform_rotations;
                    transform_rotations.SetNumUninitialized(chunk.num());
                    TArray<FVector3f, TInlineAllocator<1024>> transform_scales;
                    transform_scales.SetNumUninitialized(chunk.num());
                    for (int32 index{}; index < count; ++index) {
                        transform_positions[index] = FVector3f::ZeroVector;
                        transform_rotations[index] = FQuat4f::Identity;
                        transform_scales[index] = FVector3f::OneVector;
                        for (auto& value : chunk.custom_data(index)) {
                            value = 1.0f;
                        }
                    }
                    chunk.set_transforms<ESandboxISMCBoundsMode::Calculate>(
                        transform_positions, transform_rotations, transform_scales);
                });
        }};
        for (int32 slot{}; slot < 3; ++slot) {
            submit(1);
        }
        auto const warmed{component->get_update_metrics().staging_capacity_changes};
        component->reserve_instances(1);
        for (int32 slot{}; slot < 3; ++slot) {
            submit(4097);
        }
        TestRunner->TestEqual(TEXT("Reserved slots grow without allocating"),
                              component->get_update_metrics().staging_capacity_changes,
                              warmed);
        TestRunner->TestEqual(TEXT("Unregistered submissions have not been uploaded"),
                              component->get_update_metrics().uploads,
                              uint64{0});
    }

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
        component->set_instances(
            instance_count,
            FBox3f{FVector3f{-1000, -1000, -1000}, FVector3f{5000, 1000, 1000}},
            ESandboxISMCParallelism::Sequential,
            [&](FSandboxISMCInstanceChunkWriter& chunk) {
                auto const [first_index, chunk_count]{chunk.range()};
                chunk_offsets.Add(first_index);
                chunk_counts.Add(chunk_count);
                TArray<FVector3f, TInlineAllocator<1024>> transform_positions;
                transform_positions.SetNumUninitialized(chunk.num());
                TArray<FQuat4f, TInlineAllocator<1024>> transform_rotations;
                transform_rotations.SetNumUninitialized(chunk.num());
                TArray<FVector3f, TInlineAllocator<1024>> transform_scales;
                transform_scales.SetNumUninitialized(chunk.num());
                for (auto local_index = 0; local_index < chunk_count; ++local_index) {
                    transform_positions[local_index] = positions[first_index + local_index];
                    transform_rotations[local_index] = FQuat4f::Identity;
                    transform_scales[local_index] = FVector3f::OneVector;
                }
                chunk.set_transforms<ESandboxISMCBoundsMode::Calculate>(
                    transform_positions, transform_rotations, transform_scales);
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
                              metrics.submitted_bytes,
                              static_cast<uint64>(instance_count) *
                                  sizeof(FSandboxISMCRenderInstance));
        TestRunner->TestTrue(TEXT("Snapshot bounds are non-empty"),
                             component->CalcBounds(FTransform::Identity).SphereRadius > 0.0);
    }

    TEST_METHOD(BuildsCustomDataAlongsideTransforms)
    {
        auto* component{NewObject<USandboxISMCComponent>()};
        auto* mesh{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};
        if (!TestRunner->TestNotNull(TEXT("The engine cube mesh loads"), mesh)) {
            return;
        }
        component->set_static_mesh(*mesh);

        component->set_num_custom_data_floats(3);

        constexpr int32 instance_count{2050};
        component->set_instances(
            instance_count,
            FBox3f{FVector3f{-1000, -1000, -1000}, FVector3f{5000, 1000, 1000}},
            ESandboxISMCParallelism::Sequential,
            [&](FSandboxISMCInstanceChunkWriter& chunk) {
                auto const [first_index, chunk_count]{chunk.range()};
                TArray<FVector3f, TInlineAllocator<1024>> transform_positions;
                transform_positions.SetNumUninitialized(chunk.num());
                TArray<FQuat4f, TInlineAllocator<1024>> transform_rotations;
                transform_rotations.SetNumUninitialized(chunk.num());
                TArray<FVector3f, TInlineAllocator<1024>> transform_scales;
                transform_scales.SetNumUninitialized(chunk.num());
                for (auto local_index = 0; local_index < chunk_count; ++local_index) {
                    auto const instance_index{first_index + local_index};
                    transform_positions[local_index] = {
                        static_cast<float>(instance_index), 0.0f, 0.0f};
                    transform_rotations[local_index] = FQuat4f::Identity;
                    transform_scales[local_index] = FVector3f::OneVector;
                    auto custom_data{chunk.custom_data(local_index)};
                    custom_data[0] = static_cast<float>(instance_index);
                    custom_data[1] = 0.5f;
                    custom_data[2] = 1.0f;
                }
                chunk.set_transforms<ESandboxISMCBoundsMode::Calculate>(
                    transform_positions, transform_rotations, transform_scales);
            });

        auto const metrics{component->get_update_metrics()};
        auto const transform_bytes{static_cast<uint64>(instance_count) *
                                   sizeof(FSandboxISMCRenderInstance)};
        auto const custom_data_bytes{static_cast<uint64>(instance_count) * 3 * sizeof(float)};
        TestRunner->TestEqual(TEXT("The component retains the custom-data stride"),
                              component->get_num_custom_data_floats(),
                              3);
        TestRunner->TestEqual(TEXT("Metrics split transform upload bytes"),
                              metrics.transform_submitted_bytes,
                              transform_bytes);
        TestRunner->TestEqual(TEXT("Metrics split custom-data upload bytes"),
                              metrics.custom_data_submitted_bytes,
                              custom_data_bytes);
        TestRunner->TestEqual(TEXT("Metrics report the combined upload size"),
                              metrics.submitted_bytes,
                              transform_bytes + custom_data_bytes);
    }

    TEST_METHOD(RetainsAllStagingBuffersThroughShrinkClearAndRegrow)
    {
        auto* component{NewObject<USandboxISMCComponent>()};
        auto* mesh{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};
        if (!TestRunner->TestNotNull(TEXT("The engine cube mesh loads"), mesh)) {
            return;
        }
        component->set_static_mesh(*mesh);

        component->set_num_custom_data_floats(3);
        constexpr int32 peak_count{4097};
        constexpr int32 buffer_count{3};

        auto const submit{[&](int32 count) {
            auto written_count{0};
            component->set_instances(
                count,
                FBox3f{FVector3f{-1000, -1000, -1000}, FVector3f{5000, 1000, 1000}},
                ESandboxISMCParallelism::Sequential,
                [&](FSandboxISMCInstanceChunkWriter& chunk) {
                    auto const [first_index, chunk_count]{chunk.range()};
                    written_count += chunk_count;
                    TArray<FVector3f, TInlineAllocator<1024>> transform_positions;
                    transform_positions.SetNumUninitialized(chunk.num());
                    TArray<FQuat4f, TInlineAllocator<1024>> transform_rotations;
                    transform_rotations.SetNumUninitialized(chunk.num());
                    TArray<FVector3f, TInlineAllocator<1024>> transform_scales;
                    transform_scales.SetNumUninitialized(chunk.num());
                    for (auto local_index = 0; local_index < chunk_count; ++local_index) {
                        auto const instance_index{first_index + local_index};
                        transform_positions[local_index] = {
                            static_cast<float>(instance_index), 0.0f, 0.0f};
                        transform_rotations[local_index] = FQuat4f::Identity;
                        transform_scales[local_index] = FVector3f::OneVector;
                        auto custom_data{chunk.custom_data(local_index)};
                        custom_data[0] = static_cast<float>(instance_index);
                        custom_data[1] = 0.5f;
                        custom_data[2] = 1.0f;
                    }
                    chunk.set_transforms<ESandboxISMCBoundsMode::Calculate>(
                        transform_positions, transform_rotations, transform_scales);
                });
            TestRunner->TestEqual(TEXT("Only live instances are written"), written_count, count);
            TestRunner->TestEqual(TEXT("Retained capacity does not affect the live count"),
                                  component->get_instance_count(),
                                  count);
            auto const metrics{component->get_update_metrics()};
            TestRunner->TestEqual(TEXT("Transform upload size follows the live count"),
                                  metrics.transform_submitted_bytes,
                                  static_cast<uint64>(count) * sizeof(FSandboxISMCRenderInstance));
            TestRunner->TestEqual(TEXT("Custom-data upload size follows the live count"),
                                  metrics.custom_data_submitted_bytes,
                                  static_cast<uint64>(count) * 3 * sizeof(float));
        }};

        auto const initial_changes{component->get_update_metrics().staging_capacity_changes};
        for (auto buffer_index = 0; buffer_index < buffer_count; ++buffer_index) {
            submit(peak_count);
        }
        auto const warmed_changes{component->get_update_metrics().staging_capacity_changes};
        TestRunner->TestEqual(TEXT("Both arrays in each of the three buffers are warmed"),
                              warmed_changes - initial_changes,
                              uint64{buffer_count * 2});

        for (auto const count : {1, 0, peak_count}) {
            for (auto buffer_index = 0; buffer_index < buffer_count; ++buffer_index) {
                submit(count);
                TestRunner->TestEqual(TEXT("Shrink, zero and regrow preserve staging capacity"),
                                      component->get_update_metrics().staging_capacity_changes,
                                      warmed_changes);
            }
        }
    }

    TEST_METHOD(BuildsParallelCustomDataAcrossChunkBoundaries)
    {
        auto* component{NewObject<USandboxISMCComponent>()};
        auto* mesh{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};
        if (!TestRunner->TestNotNull(TEXT("The engine cube mesh loads"), mesh)) {
            return;
        }
        component->set_static_mesh(*mesh);

        component->set_num_custom_data_floats(3);

        constexpr int32 instance_count{4097};
        TArray<uint8> visited;
        visited.SetNumZeroed(instance_count);
        component->set_instances(
            instance_count,
            FBox3f{FVector3f{-1000, -1000, -1000}, FVector3f{5000, 1000, 1000}},
            ESandboxISMCParallelism::Parallel,
            [&](FSandboxISMCInstanceChunkWriter& chunk) {
                auto const [first_index, chunk_count]{chunk.range()};
                TArray<FVector3f, TInlineAllocator<1024>> transform_positions;
                transform_positions.SetNumUninitialized(chunk.num());
                TArray<FQuat4f, TInlineAllocator<1024>> transform_rotations;
                transform_rotations.SetNumUninitialized(chunk.num());
                TArray<FVector3f, TInlineAllocator<1024>> transform_scales;
                transform_scales.SetNumUninitialized(chunk.num());
                for (auto local_index = 0; local_index < chunk_count; ++local_index) {
                    auto const instance_index{first_index + local_index};
                    ++visited[instance_index];
                    transform_positions[local_index] = {
                        static_cast<float>(instance_index), 0.0f, 0.0f};
                    transform_rotations[local_index] = FQuat4f::Identity;
                    transform_scales[local_index] = FVector3f::OneVector;
                    auto custom_data{chunk.custom_data(local_index)};
                    custom_data[0] = static_cast<float>(instance_index);
                    custom_data[1] = static_cast<float>(first_index);
                    custom_data[2] = static_cast<float>(local_index);
                }
                chunk.set_transforms<ESandboxISMCBoundsMode::Calculate>(
                    transform_positions, transform_rotations, transform_scales);
            });

        auto every_instance_visited_once{true};
        for (auto const visit_count : visited) {
            every_instance_visited_once &= visit_count == 1;
        }
        TestRunner->TestTrue(TEXT("Parallel custom-data chunks cover every instance exactly once"),
                             every_instance_visited_once);
        TestRunner->TestEqual(TEXT("Parallel custom data retains the complete snapshot"),
                              component->get_instance_count(),
                              instance_count);
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
            4,
            FBox3f{FVector3f{-1000, -1000, -1000}, FVector3f{5000, 1000, 1000}},
            ESandboxISMCParallelism::Sequential,
            [&](FSandboxISMCInstanceChunkWriter& chunk) {
                TArray<FVector3f, TInlineAllocator<1024>> transform_positions;
                transform_positions.SetNumUninitialized(chunk.num());
                TArray<FQuat4f, TInlineAllocator<1024>> transform_rotations;
                transform_rotations.SetNumUninitialized(chunk.num());
                TArray<FVector3f, TInlineAllocator<1024>> transform_scales;
                transform_scales.SetNumUninitialized(chunk.num());
                for (auto index = 0; index < chunk.num(); ++index) {
                    transform_positions[index] = {static_cast<float>(index), 0.0f, 0.0f};
                    transform_rotations[index] = FQuat4f::Identity;
                    transform_scales[index] = FVector3f::OneVector;
                }
                chunk.set_transforms<ESandboxISMCBoundsMode::Calculate>(
                    transform_positions, transform_rotations, transform_scales);
            });
        component->set_instances(
            1,
            FBox3f{FVector3f{-1000, -1000, -1000}, FVector3f{5000, 1000, 1000}},
            ESandboxISMCParallelism::Sequential,
            [&](FSandboxISMCInstanceChunkWriter& chunk) {
                {
                    FVector3f const transform_positions[]{{100.0f, 0.0f, 0.0f}};
                    FQuat4f const transform_rotations[]{FQuat4f::Identity};
                    FVector3f const transform_scales[]{FVector3f::OneVector};
                    chunk.set_transforms<ESandboxISMCBoundsMode::Calculate>(
                        transform_positions, transform_rotations, transform_scales);
                }
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
            0,
            FBox3f{FVector3f{-1000, -1000, -1000}, FVector3f{5000, 1000, 1000}},
            ESandboxISMCParallelism::Parallel,
            [&](FSandboxISMCInstanceChunkWriter&) { callback_invoked = true; });
        TestRunner->TestFalse(TEXT("Empty snapshots do not invoke the callback"), callback_invoked);

        component->set_instances(
            1,
            FBox3f{FVector3f{-1000, -1000, -1000}, FVector3f{5000, 1000, 1000}},
            ESandboxISMCParallelism::Sequential,
            [&](FSandboxISMCInstanceChunkWriter& chunk) {
                {
                    FVector3f const transform_positions[]{FVector3f::ZeroVector};
                    FQuat4f const transform_rotations[]{FQuat4f::Identity};
                    FVector3f const transform_scales[]{FVector3f::OneVector};
                    chunk.set_transforms<ESandboxISMCBoundsMode::Calculate>(
                        transform_positions, transform_rotations, transform_scales);
                }
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
                instance_count,
                FBox3f{FVector3f{-1000, -1000, -1000}, FVector3f{5000, 1000, 1000}},
                parallelism,
                [&](FSandboxISMCInstanceChunkWriter& chunk) {
                    auto const [first_index, chunk_count]{chunk.range()};
                    TArray<FVector3f, TInlineAllocator<1024>> transform_positions;
                    transform_positions.SetNumUninitialized(chunk.num());
                    TArray<FQuat4f, TInlineAllocator<1024>> transform_rotations;
                    transform_rotations.SetNumUninitialized(chunk.num());
                    TArray<FVector3f, TInlineAllocator<1024>> transform_scales;
                    transform_scales.SetNumUninitialized(chunk.num());
                    for (auto local_index = 0; local_index < chunk_count; ++local_index) {
                        auto const source_index{first_index + local_index};
                        transform_positions[local_index] = positions[source_index];
                        transform_rotations[local_index] = rotations[source_index].Quaternion();
                        transform_scales[local_index] = FVector3f::OneVector;
                    }
                    chunk.set_transforms<ESandboxISMCBoundsMode::Calculate>(
                        transform_positions, transform_rotations, transform_scales);
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

    TEST_METHOD(UsesCallerSuppliedBounds)
    {
        auto* component{NewObject<USandboxISMCComponent>()};
        auto* mesh{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};
        if (!TestRunner->TestNotNull(TEXT("The engine cube mesh loads"), mesh)) {
            return;
        }
        component->set_static_mesh(*mesh);

        FBox3f const local_bounds{FVector3f{-100.0f, -120.0f, -130.0f},
                                  FVector3f{140.0f, 150.0f, 160.0f}};

        component->set_instances(
            2,
            FBox3f{FVector3f{-1000, -1000, -1000}, FVector3f{5000, 1000, 1000}},
            local_bounds,
            ESandboxISMCParallelism::Sequential,
            [&](FSandboxISMCInstanceChunkWriter& chunk) {
                FVector3f const positions[]{{-8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f}};
                FQuat4f const rotations[]{FQuat4f::Identity, FQuat4f::Identity};
                FVector3f const scales[]{FVector3f::OneVector, FVector3f::OneVector};
                chunk.set_transforms<ESandboxISMCBoundsMode::Supplied>(
                    positions, rotations, scales);
            });

        auto const actual{component->CalcBounds(FTransform::Identity)};
        auto const expected{FBoxSphereBounds{
            FBoxSphereBounds3f{FSandboxISMCInstanceChunkWriter::expand_render_bounds(
                local_bounds,
                FVector3f{mesh->GetBounds().Origin},
                FVector3f{mesh->GetBounds().BoxExtent})}}};
        TestRunner->TestTrue(TEXT("The component uses the supplied bounds origin"),
                             actual.Origin.Equals(expected.Origin));
        TestRunner->TestTrue(TEXT("The component uses the supplied bounds extent"),
                             actual.BoxExtent.Equals(expected.BoxExtent));
        TestRunner->TestEqual(TEXT("The component uses the supplied sphere radius"),
                              actual.SphereRadius,
                              expected.SphereRadius);

        component->set_instances(
            0,
            FBox3f{FVector3f{-1000, -1000, -1000}, FVector3f{5000, 1000, 1000}},
            local_bounds,
            ESandboxISMCParallelism::Sequential,
            [](FSandboxISMCInstanceChunkWriter&) {});
        TestRunner->TestEqual(TEXT("An empty snapshot clears supplied bounds"),
                              component->CalcBounds(FTransform::Identity).SphereRadius,
                              0.0);
    }

    TEST_METHOD(SuppliedSourceBoundsContainDecodedGeometryAtRoundingExtremes)
    {
        auto* component{NewObject<USandboxISMCComponent>()};
        auto* cube{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};
        if (!TestRunner->TestNotNull(TEXT("Cube loads"), cube)) {
            return;
        }
        component->set_static_mesh(*cube);
        auto const mesh_box{FBox3f{cube->GetBounds().GetBox()}};
        for (auto const position : {FVector3f{-8, 8, -24}, FVector3f{8, -8, 24}}) {
            auto const rotation{FRotator3f{31, -123, 179.99f}.Quaternion()};
            FVector3f const scale{31.8125f, 0.0625f, 1.1875f};
            FTransform3f const source{rotation, position, scale};
            auto const source_bounds{mesh_box.TransformBy(source.ToMatrixWithScale())};
            FBox3f const domain{FVector3f{-32}, FVector3f{32}};
            FSandboxISMCRenderInstance packed{};
            component->set_instances(
                1,
                domain,
                source_bounds,
                ESandboxISMCParallelism::Sequential,
                [&](FSandboxISMCInstanceChunkWriter& writer) {
                    {
                        FVector3f const transform_positions[]{position};
                        FQuat4f const transform_rotations[]{rotation};
                        FVector3f const transform_scales[]{scale};
                        writer.set_transforms<ESandboxISMCBoundsMode::Supplied>(
                            transform_positions, transform_rotations, transform_scales);
                    }
                    FSandboxISMCInstanceChunkWriter reference{MakeArrayView(&packed, 1),
                                                              {},
                                                              0,
                                                              0,
                                                              domain,
                                                              FVector3f::ZeroVector,
                                                              FVector3f::ZeroVector,
                                                              FVector3f::ZeroVector};
                    {
                        FVector3f const transform_positions[]{position};
                        FQuat4f const transform_rotations[]{rotation};
                        FVector3f const transform_scales[]{scale};
                        reference.set_transforms<ESandboxISMCBoundsMode::Supplied>(
                            transform_positions, transform_rotations, transform_scales);
                    }
                });
            auto const q{ml::sandbox_ismc::unpack_quat32(packed.rotation)};
            FTransform3f const decoded{
                FQuat4f{q.X, q.Y, q.Z, q.W},
                FVector3f{packed.position[0] * 16.0f,
                          packed.position[1] * 16.0f,
                          packed.position[2] * 16.0f},
                FVector3f{static_cast<float>(packed.scale[0].scale_value()),
                          static_cast<float>(packed.scale[1].scale_value()),
                          static_cast<float>(packed.scale[2].scale_value())}};
            auto const actual{component->CalcBounds(FTransform::Identity).GetBox()};
            for (int32 corner{0}; corner < 8; ++corner) {
                FVector3f const point{(corner & 1) ? mesh_box.Max.X : mesh_box.Min.X,
                                      (corner & 2) ? mesh_box.Max.Y : mesh_box.Min.Y,
                                      (corner & 4) ? mesh_box.Max.Z : mesh_box.Min.Z};
                TestRunner->TestTrue(
                    TEXT("Codec expansion contains decoded supplied-bound geometry"),
                    actual.IsInsideOrOn(FVector{decoded.TransformPosition(point)}));
            }
        }
    }

    TEST_METHOD(SupportsChunkAndParallelismBoundaries)
    {
        auto* component{NewObject<USandboxISMCComponent>()};
        auto* mesh{LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))};
        if (!TestRunner->TestNotNull(TEXT("The engine cube mesh loads"), mesh)) {
            return;
        }
        component->set_static_mesh(*mesh);

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
                    count,
                    FBox3f{FVector3f{-1000, -1000, -1000}, FVector3f{5000, 1000, 1000}},
                    policy,
                    [&](FSandboxISMCInstanceChunkWriter& chunk) {
                        auto const [first_index, chunk_count]{chunk.range()};
                        TArray<FVector3f, TInlineAllocator<1024>> transform_positions;
                        transform_positions.SetNumUninitialized(chunk.num());
                        TArray<FQuat4f, TInlineAllocator<1024>> transform_rotations;
                        transform_rotations.SetNumUninitialized(chunk.num());
                        TArray<FVector3f, TInlineAllocator<1024>> transform_scales;
                        transform_scales.SetNumUninitialized(chunk.num());
                        for (auto local_index = 0; local_index < chunk_count; ++local_index) {
                            auto const source_index{first_index + local_index};
                            ++visited[source_index];
                            transform_positions[local_index] = {
                                static_cast<float>(source_index), 0.0f, 0.0f};
                            transform_rotations[local_index] = FQuat4f::Identity;
                            transform_scales[local_index] = FVector3f::OneVector;
                        }
                        chunk.set_transforms<ESandboxISMCBoundsMode::Calculate>(
                            transform_positions, transform_rotations, transform_scales);
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
