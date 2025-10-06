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

    AMassBulletVisualizationActor* actor{nullptr};

    auto executor{[&actor](FMassExecutionContext& context, auto& Data) {
        TRACE_CPUPROFILER_EVENT_SCOPE(
            TEXT("Sandbox::FMassBulletVisualizationExecutor::Execute::executor"))

        auto const n{context.GetNumEntities()};
        auto const viz_fragment{
            context.GetConstSharedFragment<FMassBulletVisualizationActorFragment>()};
        RETURN_IF_NULLPTR(viz_fragment.actor);

        auto const state_fragments{context.GetFragmentView<FMassBulletStateFragment>()};

        auto const transforms{context.GetFragmentView<FMassBulletTransformFragment>()};
        auto const indices{context.GetFragmentView<FMassBulletInstanceIndexFragment>()};

        if (!actor) {
            actor = viz_fragment.actor;
        }

        for (int32 i{0}; i < n; ++i) {
            if (state_fragments[i].state != EBulletState::Active) {
                continue;
            }

            viz_fragment.actor->update_instance(indices[i].instance_index, transforms[i].transform);
        }
    }};

    ForEachEntityChunk(context, accessors, std::move(executor));

    // RETURN_IF_NULLPTR(actor);
    if (!actor) {
        return;
    }

    if (!actor->mark_render_state_as_dirty()) {
        logger.log_error(TEXT("Failed to mark render state as dirty."));
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
