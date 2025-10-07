#include "Sandbox/mass_entity/processors/MassBulletVisualizationProcessor.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassProcessingTypes.h"

#include "Sandbox/actors/MassBulletVisualizationActor.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

#include "Sandbox/macros/null_checks.hpp"

void FMassBulletVisualizationExecutor::Execute(FMassExecutionContext& context) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletVisualizationExecutor::Execute"))
    constexpr auto logger{NestedLogger<"Execute">()};

    // First pass: collect all entity data
    struct EntityData {
        int32 instance_index;
        FTransform transform;
    };

    TArray<EntityData> entity_data_array{};

    auto collector{[&entity_data_array](FMassExecutionContext& context, auto& Data) {
        auto const n{context.GetNumEntities()};
        auto const viz_fragment{
            context.GetConstSharedFragment<FMassBulletVisualizationActorFragment>()};
        RETURN_IF_NULLPTR(viz_fragment.actor);

        auto const transforms{context.GetFragmentView<FMassBulletTransformFragment>()};
        auto const states{context.GetFragmentView<FMassBulletStateFragment>()};
        auto const indices{context.GetFragmentView<FMassBulletInstanceIndexFragment>()};
        auto const& hidden_transform{viz_fragment.actor->get_hidden_transform()};

        for (int32 i{0}; i < n; ++i) {
            auto const& transform{states[i].hit_occurred ? hidden_transform
                                                         : transforms[i].transform};
            entity_data_array.Add({indices[i].instance_index, transform});
        }
    }};

    ForEachEntityChunk(context, accessors, std::move(collector));

    // Sort by instance index
    entity_data_array.Sort([](EntityData const& a, EntityData const& b) {
        return a.instance_index < b.instance_index;
    });

    // Enqueue transforms in instance index order
    auto const viz_fragment{
        context.GetConstSharedFragment<FMassBulletVisualizationActorFragment>()};
    if (viz_fragment.actor) {
        auto& transform_queue{viz_fragment.actor->get_transform_queue()};
        for (auto const& data : entity_data_array) {
            (void)transform_queue.enqueue(data.transform);
        }
    }
}

UMassBulletVisualizationProcessor::UMassBulletVisualizationProcessor()
    : entity_query(*this) {
    executor =
        UE::Mass::FQueryExecutor::CreateQuery<FMassBulletVisualizationExecutor>(entity_query, this);
    AutoExecuteQuery = executor;

    SetProcessingPhase(EMassProcessingPhase::FrameEnd);

    if (HasAnyFlags(RF_ClassDefaultObject)) {
        bRequiresGameThreadExecution = true;
        set_execution_flags(EProcessorExecutionFlags::All);
    }
}
