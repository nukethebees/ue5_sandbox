#include "Sandbox/mass_entity/processors/MassBulletDestructionProcessor.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

#include "Sandbox/actors/MassBulletVisualizationActor.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/mass_entity/processors/BulletProcessorGroups.h"

#include "Sandbox/macros/null_checks.hpp"

void FMassBulletDestructionExecutor::Execute(FMassExecutionContext& context) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletDestructionExecutor::Execute"))
    constexpr auto logger{NestedLogger<"Execute">()};

    auto executor{[](FMassExecutionContext& context, auto& Data) {
        auto const n{context.GetNumEntities()};
        auto const state_fragments{context.GetFragmentView<FMassBulletStateFragment>()};
        auto const viz_fragment{
            context.GetConstSharedFragment<FMassBulletVisualizationActorFragment>()};
        RETURN_IF_NULLPTR(viz_fragment.actor);

        auto const indices{context.GetFragmentView<FMassBulletInstanceIndexFragment>()};

        for (int32 i{0}; i < n; ++i) {
            if (!state_fragments[i].hit_occurred) {
                continue;
            }

            auto const instance_index{indices[i].instance_index};
            viz_fragment.actor->return_instance_to_pool(instance_index);

            auto entity{context.GetEntity(i)};
            context.Defer().DestroyEntity(entity);
        }
    }};

    ForEachEntityChunk(context, accessors, std::move(executor));
}

UMassBulletDestructionProcessor::UMassBulletDestructionProcessor()
    : entity_query(*this) {
    executor =
        UE::Mass::FQueryExecutor::CreateQuery<FMassBulletDestructionExecutor>(entity_query, this);
    AutoExecuteQuery = executor;

    SetProcessingPhase(EMassProcessingPhase::FrameEnd);
    ExecutionOrder.ExecuteAfter.Add(ml::ProcessorGroupNames::CollisionVisualization);

    if (HasAnyFlags(RF_ClassDefaultObject)) {
        bRequiresGameThreadExecution = true;
        set_execution_flags(EProcessorExecutionFlags::All);
    }
}
