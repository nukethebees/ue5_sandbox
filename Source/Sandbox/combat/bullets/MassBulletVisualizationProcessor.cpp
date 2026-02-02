#include "Sandbox/combat/bullets/MassBulletVisualizationProcessor.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassProcessingTypes.h"

#include "Sandbox/combat/bullets/BulletProcessorGroups.h"
#include "Sandbox/combat/bullets/MassBulletFragments.h"
#include "Sandbox/combat/bullets/MassBulletVisualizationActor.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

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

        auto const& data_fragment{context.GetConstSharedFragment<FMassBulletDataFragment>()};

        for (int32 i{0}; i < n; ++i) {
            if (states[i].hit_occurred) {
                viz_fragment.actor->increment_killed_count(data_fragment.bullet_type_index);
            } else {
                viz_fragment.actor->enqueue_transform(data_fragment.bullet_type_index,
                                                      transforms[i].transform);
            }
        }
    }};

    ForEachEntityChunk(context, accessors, std::move(executor));
}

UMassBulletVisualizationProcessor::UMassBulletVisualizationProcessor()
    : entity_query(*this) {
    executor =
        UE::Mass::FQueryExecutor::CreateQuery<FMassBulletVisualizationExecutor>(entity_query, this);
    AutoExecuteQuery = executor;

    SetProcessingPhase(EMassProcessingPhase::PostPhysics);
    ExecutionOrder.ExecuteAfter.Add(ml::ProcessorGroupNames::CollisionHandling);
    ExecutionOrder.ExecuteInGroup = ml::ProcessorGroupNames::CollisionVisualization;

    if (HasAnyFlags(RF_ClassDefaultObject)) {
        bRequiresGameThreadExecution = true;
        set_execution_flags(EProcessorExecutionFlags::All);
    }
}
