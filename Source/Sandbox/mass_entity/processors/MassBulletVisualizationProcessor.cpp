#include "Sandbox/mass_entity/processors/MassBulletVisualizationProcessor.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassProcessingTypes.h"

#include "Sandbox/actors/MassBulletVisualizationActor.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/mass_entity/processors/BulletProcessorGroups.h"

#include "Sandbox/macros/null_checks.hpp"

void FMassBulletVisualizationExecutor::Execute(FMassExecutionContext& context) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletVisualizationExecutor::Execute"))
    constexpr auto logger{NestedLogger<"Execute">()};

    constexpr auto executor{[](FMassExecutionContext& context, auto& Data) {
        TRACE_CPUPROFILER_EVENT_SCOPE(
            TEXT("Sandbox::FMassBulletVisualizationExecutor::Execute::executor"))
        auto const n{context.GetNumEntities()};
        auto const viz_fragment{
            context.GetConstSharedFragment<FMassBulletVisualizationActorFragment>()};
        RETURN_IF_NULLPTR(viz_fragment.actor);

        auto const transforms{context.GetFragmentView<FMassBulletTransformFragment>()};
        auto const states{context.GetFragmentView<FMassBulletStateFragment>()};
        auto const& hidden_transform{viz_fragment.actor->get_hidden_transform()};
        for (int32 i{0}; i < n; ++i) {
            auto const& transform{states[i].hit_occurred ? hidden_transform
                                                         : transforms[i].transform};
            viz_fragment.actor->enqueue_transform(transform);
        }
    }};

    ForEachEntityChunk(context, accessors, std::move(executor));
}

UMassBulletVisualizationProcessor::UMassBulletVisualizationProcessor()
    : entity_query(*this) {
    executor =
        UE::Mass::FQueryExecutor::CreateQuery<FMassBulletVisualizationExecutor>(entity_query, this);
    AutoExecuteQuery = executor;

    SetProcessingPhase(EMassProcessingPhase::FrameEnd);
    ExecutionOrder.ExecuteAfter.Add(ml::ProcessorGroupNames::CollisionHandling);
    ExecutionOrder.ExecuteInGroup = ml::ProcessorGroupNames::CollisionVisualization;

    if (HasAnyFlags(RF_ClassDefaultObject)) {
        bRequiresGameThreadExecution = true;
        set_execution_flags(EProcessorExecutionFlags::All);
    }
}
