#include "SandboxISMCInstanceState.h"

#include <CQTest.h>

namespace {
auto read_instances(ml::sandbox_ismc::InstanceState const& state)
    -> ml::sandbox_ismc::InstanceDataConstView {
    return state.instances();
}

void test_vector(FAutomationTestBase& test,
                 TCHAR const* const message,
                 FVector3f const& actual,
                 FVector3f const& expected) {
    test.TestTrue(message, actual.Equals(expected, UE_KINDA_SMALL_NUMBER));
}

void test_render_range(FAutomationTestBase& test,
                       FSandboxISMCRenderRange const& range,
                       int32 first_instance,
                       int32 count) {
    test.TestEqual(
        TEXT("Range starts at the expected instance"), range.first_instance, first_instance);
    test.TestEqual(TEXT("Range contains the expected instance count"), range.count, count);
}
}

TEST_CLASS(SandboxISMCInstanceStorage, "SandboxISMC.UnitTests")
{
    TEST_METHOD(AddsInstancesAndSuppliesDefaults)
    {
        ml::sandbox_ismc::InstanceState state;
        TArray<FVector3f> const positions{{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}};

        TestRunner->TestEqual(TEXT("Empty adds return no index"),
                              state.add_instances(TConstArrayView<FVector3f>{}),
                              INDEX_NONE);
        TestRunner->TestEqual(
            TEXT("The first batch starts at zero"), state.add_instances(positions), 0);

        auto const instances{read_instances(state)};
        TestRunner->TestEqual(TEXT("Both rows are stored"), instances.num(), 2);
        test_vector(
            *TestRunner, TEXT("Positions are copied"), instances.positions[1], positions[1]);
        TestRunner->TestTrue(TEXT("Rotations default to identity"),
                             instances.rotations[0].Equals(FQuat4f::Identity));
        test_vector(
            *TestRunner, TEXT("Scales default to one"), instances.scales[0], FVector3f::OneVector);
    }

    TEST_METHOD(AddsCompleteRowsAndUpdatesContiguousAndScatteredRows)
    {
        ml::sandbox_ismc::InstanceData source;
        source.positions = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}};
        source.rotations = {FQuat4f::Identity, FQuat4f::Identity, FQuat4f::Identity};
        source.scales = {{1.0f, 2.0f, 3.0f}, {2.0f, 3.0f, 4.0f}, {3.0f, 4.0f, 5.0f}};

        ml::sandbox_ismc::InstanceState state;
        TestRunner->TestEqual(
            TEXT("Complete rows start at zero"), state.add_instances(source.get_const_view()), 0);

        ml::sandbox_ismc::InstanceData contiguous;
        contiguous.positions = {{10.0f, 0.0f, 0.0f}, {11.0f, 0.0f, 0.0f}};
        contiguous.rotations = {FQuat4f::Identity, FQuat4f::Identity};
        contiguous.scales = {{10.0f, 1.0f, 1.0f}, {11.0f, 1.0f, 1.0f}};
        state.set_instance_transforms(1, contiguous.get_const_view());

        ml::sandbox_ismc::InstanceData scattered;
        scattered.positions = {{20.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 0.0f}, {40.0f, 0.0f, 0.0f}};
        scattered.rotations = {FQuat4f::Identity, FQuat4f::Identity, FQuat4f::Identity};
        scattered.scales = {{20.0f, 1.0f, 1.0f}, {30.0f, 1.0f, 1.0f}, {40.0f, 1.0f, 1.0f}};
        TArray<int32> const indices{2, 0, 2};
        state.set_instance_transforms(indices, scattered.get_const_view());

        auto const instances{read_instances(state)};
        test_vector(*TestRunner,
                    TEXT("Scattered rows update their requested index"),
                    instances.positions[0],
                    scattered.positions[1]);
        test_vector(*TestRunner,
                    TEXT("The last duplicate scattered update wins"),
                    instances.positions[2],
                    scattered.positions[2]);
        test_vector(*TestRunner,
                    TEXT("SOA fields remain aligned"),
                    instances.scales[2],
                    scattered.scales[2]);
    }

    TEST_METHOD(AddsPositionAndRotationBatches)
    {
        ml::sandbox_ismc::InstanceState state;
        TArray<FVector3f> const positions{{1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}};
        TArray<FQuat4f> const rotations{FQuat4f::Identity,
                                        FQuat4f{FVector3f::UpVector, UE_HALF_PI}};

        TestRunner->TestEqual(TEXT("Position and rotation batches return their first index"),
                              state.add_instances(positions, rotations),
                              0);

        auto const instances{read_instances(state)};
        TestRunner->TestTrue(TEXT("Explicit rotations are copied"),
                             instances.rotations[1].Equals(rotations[1]));
        test_vector(*TestRunner,
                    TEXT("Position and rotation batches default scale"),
                    instances.scales[1],
                    FVector3f::OneVector);
    }

    TEST_METHOD(RemovesRowsWithSwapSemanticsAndClearsStorage)
    {
        ml::sandbox_ismc::InstanceState state;
        TArray<FVector3f> const positions{{0.0f, 0.0f, 0.0f},
                                          {1.0f, 0.0f, 0.0f},
                                          {2.0f, 0.0f, 0.0f},
                                          {3.0f, 0.0f, 0.0f},
                                          {4.0f, 0.0f, 0.0f}};
        state.add_instances(positions);

        TArray<int32> const no_indices;
        state.remove_instances_swap(no_indices);
        TestRunner->TestEqual(
            TEXT("Empty removal batches do nothing"), state.get_instance_count(), 5);

        TArray<int32> const indices{3, 1};
        state.remove_instances_swap(indices);
        auto const remaining{read_instances(state)};
        TestRunner->TestEqual(TEXT("Both rows are removed"), remaining.num(), 3);
        test_vector(*TestRunner,
                    TEXT("First retained row is unchanged"),
                    remaining.positions[0],
                    positions[0]);
        test_vector(*TestRunner,
                    TEXT("Swap removal fills the second row"),
                    remaining.positions[1],
                    positions[4]);
        test_vector(*TestRunner,
                    TEXT("Last retained row is unchanged"),
                    remaining.positions[2],
                    positions[2]);

        state.clear_instances();
        TestRunner->TestTrue(TEXT("Clear removes every row"), read_instances(state).is_empty());
    }
};

TEST_CLASS(SandboxISMCUpdatePreparation, "SandboxISMC.UnitTests")
{
    TEST_METHOD(CoalescesDirtyRangesAndPacksThemInIndexOrder)
    {
        ml::sandbox_ismc::InstanceState state;
        TArray<FVector3f> positions;
        for (auto index = 0; index < 10; ++index) {
            positions.Add({static_cast<float>(index), 0.0f, 0.0f});
        }
        state.add_instances(positions);
        state.prepare_update(false);

        state.mark_instance_range_dirty(5, 2);
        state.mark_instance_range_dirty(1, 2);
        state.mark_instance_range_dirty(3, 2);
        state.mark_instance_range_dirty(2, 3);
        state.mark_instance_range_dirty(9, 1);
        auto const prepared{state.prepare_update(false)};

        TestRunner->TestTrue(TEXT("Dirty state creates a packet"),
                             prepared.render_update.IsValid());
        TestRunner->TestFalse(TEXT("The packet remains partial"),
                              prepared.render_update->full_upload);
        TestRunner->TestEqual(TEXT("Overlapping and adjacent ranges coalesce"),
                              prepared.render_update->ranges.Num(),
                              2);
        test_render_range(*TestRunner, prepared.render_update->ranges[0], 1, 6);
        test_render_range(*TestRunner, prepared.render_update->ranges[1], 9, 1);
        TestRunner->TestEqual(
            TEXT("Metrics count unique dirty instances"), prepared.dirty_instance_count, 7);
        TestRunner->TestEqual(
            TEXT("Metrics count coalesced ranges"), prepared.dirty_range_count, 2);
        TestRunner->TestEqual(TEXT("Packed instances follow range order"),
                              prepared.render_update->instances[0].origin.X,
                              1.0f);
        TestRunner->TestEqual(TEXT("The second range follows the first"),
                              prepared.render_update->instances.Last().origin.X,
                              9.0f);
    }

    TEST_METHOD(ChoosesFullAndPartialUploadsAndSkipsUnchangedState)
    {
        ml::sandbox_ismc::InstanceState state;
        TArray<FVector3f> const initial{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
        state.add_instances(initial);

        auto const first{state.prepare_update(false)};
        TestRunner->TestTrue(TEXT("The initial packet is a full upload"),
                             first.render_update->full_upload);
        TestRunner->TestFalse(TEXT("Unchanged state produces no packet"),
                              state.prepare_update(false).render_update.IsValid());

        state.mark_instance_range_dirty(1, 1);
        auto const partial{state.prepare_update(false)};
        TestRunner->TestFalse(TEXT("An in-capacity update is partial"),
                              partial.render_update->full_upload);

        state.mark_instance_range_dirty(1, 1);
        auto const invalid_snapshot{state.prepare_update(true)};
        TestRunner->TestTrue(TEXT("An invalid render snapshot forces a full upload"),
                             invalid_snapshot.render_update->full_upload);

        TArray<FVector3f> const growth{{2.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f}};
        state.add_instances(growth);
        auto const grown{state.prepare_update(false)};
        TestRunner->TestTrue(TEXT("Growth beyond submitted capacity forces a full upload"),
                             grown.render_update->full_upload);

        state.clear_instances();
        auto const cleared{state.prepare_update(false)};
        TestRunner->TestTrue(TEXT("Clearing creates a packet"), cleared.render_update.IsValid());
        TestRunner->TestEqual(
            TEXT("Clear reports zero instances"), cleared.render_update->instance_count, 0);
        TestRunner->TestTrue(TEXT("Clear uses a full upload"), cleared.render_update->full_upload);
    }

    TEST_METHOD(PacksOriginsAndTransformRows)
    {
        ml::sandbox_ismc::InstanceData instances;
        instances.positions = {{4.0f, 5.0f, 6.0f}, {7.0f, 8.0f, 9.0f}};
        instances.rotations = {FQuat4f::Identity, FQuat4f{FVector3f::UpVector, UE_HALF_PI}};
        instances.scales = {{2.0f, 3.0f, 4.0f}, {1.0f, 2.0f, 3.0f}};

        ml::sandbox_ismc::InstanceState state;
        state.add_instances(instances.get_const_view());
        auto const prepared{state.prepare_update(false)};

        TestRunner->TestEqual(
            TEXT("One full range is emitted"), prepared.render_update->ranges.Num(), 1);
        test_render_range(*TestRunner, prepared.render_update->ranges[0], 0, 2);
        TestRunner->TestEqual(TEXT("Render rows remain 64 bytes"),
                              static_cast<int32>(sizeof(FSandboxISMCRenderInstance)),
                              64);

        auto const source{instances.get_const_view()};
        for (auto index = 0; index < source.num(); ++index) {
            auto const matrix{
                FTransform3f{source.rotations[index], source.positions[index], source.scales[index]}
                    .ToMatrixWithScale()};
            auto const& packed{prepared.render_update->instances[index]};
            TestRunner->TestTrue(TEXT("Packed origins preserve position"),
                                 packed.origin.Equals(FVector4f{source.positions[index], 0.0f}));
            TestRunner->TestTrue(TEXT("First transform rows match the source transform"),
                                 packed.transform_row_0.Equals(FVector4f{
                                     matrix.M[0][0], matrix.M[0][1], matrix.M[0][2], 0.0f}));
            TestRunner->TestTrue(TEXT("Second transform rows match the source transform"),
                                 packed.transform_row_1.Equals(FVector4f{
                                     matrix.M[1][0], matrix.M[1][1], matrix.M[1][2], 0.0f}));
            TestRunner->TestTrue(TEXT("Third transform rows match the source transform"),
                                 packed.transform_row_2.Equals(FVector4f{
                                     matrix.M[2][0], matrix.M[2][1], matrix.M[2][2], 0.0f}));
        }
    }
};

TEST_CLASS(SandboxISMCBounds, "SandboxISMC.UnitTests")
{
    TEST_METHOD(ComputesConservativeBoundsFromMeshSphereAndTransforms)
    {
        ml::sandbox_ismc::InstanceState state;
        state.set_mesh_bounds({1.0f, 0.0f, 0.0f}, 2.0f);

        ml::sandbox_ismc::InstanceData instance;
        instance.positions = {{10.0f, 0.0f, 0.0f}};
        instance.rotations = {FQuat4f{FVector3f::UpVector, UE_HALF_PI}};
        instance.scales = {{-2.0f, 1.0f, 1.0f}};
        state.add_instances(instance.get_const_view());
        state.prepare_update(false);

        auto const bounds{state.local_bounds()};
        test_vector(*TestRunner,
                    TEXT("The scaled mesh origin is rotated and translated"),
                    FVector3f{bounds.Origin},
                    {10.0f, -2.0f, 0.0f});
        test_vector(*TestRunner,
                    TEXT("The largest absolute scale controls conservative extent"),
                    FVector3f{bounds.BoxExtent},
                    {4.0f, 4.0f, 4.0f});
    }

    TEST_METHOD(UpdatesSparseBoundsAfterFullRangeFastPath)
    {
        ml::sandbox_ismc::InstanceState state;
        state.set_mesh_bounds(FVector3f::ZeroVector, 1.0f);
        TArray<FVector3f> const positions{{0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}};
        state.add_instances(positions);
        state.prepare_update(false);

        auto all{state.instances()};
        all.positions[0] = {2.0f, 0.0f, 0.0f};
        all.positions[1] = {12.0f, 0.0f, 0.0f};
        state.prepare_update(false);

        auto one{state.edit_instances(1, 1)};
        one.positions[0] = {20.0f, 0.0f, 0.0f};
        state.prepare_update(false);

        auto const bounds{state.local_bounds()};
        test_vector(*TestRunner,
                    TEXT("Sparse updates rebuild an invalidated bounds tree"),
                    FVector3f{bounds.Origin},
                    {11.0f, 0.0f, 0.0f});
        test_vector(*TestRunner,
                    TEXT("Sparse updates retain every instance in the bounds"),
                    FVector3f{bounds.BoxExtent},
                    {10.0f, 1.0f, 1.0f});

        state.clear_instances();
        state.prepare_update(false);
        TestRunner->TestEqual(
            TEXT("Empty state has zero sphere radius"), state.local_bounds().SphereRadius, 0.0);
    }

    TEST_METHOD(RebuildsBoundsAfterSwapRemoval)
    {
        ml::sandbox_ismc::InstanceState state;
        state.set_mesh_bounds(FVector3f::ZeroVector, 1.0f);
        TArray<FVector3f> const positions{
            {0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}};
        state.add_instances(positions);
        state.prepare_update(false);

        TArray<int32> const removed{2};
        state.remove_instances_swap(removed);
        state.prepare_update(false);

        auto const bounds{state.local_bounds()};
        test_vector(*TestRunner,
                    TEXT("Removed rows no longer contribute to bounds"),
                    FVector3f{bounds.Origin},
                    {5.0f, 0.0f, 0.0f});
        test_vector(*TestRunner,
                    TEXT("Retained rows still contribute to bounds"),
                    FVector3f{bounds.BoxExtent},
                    {6.0f, 1.0f, 1.0f});
    }
};
