#include "Sandbox/mass_entity/processors/MassBulletVisualizationProcessor.h"

#include "EngineUtils.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassProcessingTypes.h"

#include "Sandbox/actors/MassBulletVisualizationActor.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

#include "Sandbox/macros/null_checks.hpp"

void FMassBulletVisualizationExecutor::Execute(FMassExecutionContext& context) {
    TRY_INIT_PTR(world, context.GetWorld());

    AMassBulletVisualizationActor* visualization_actor{nullptr};
    for (auto it{TActorIterator<AMassBulletVisualizationActor>(world)}; it; ++it) {
        visualization_actor = *it;
        break;
    }

    RETURN_IF_NULLPTR(visualization_actor);

    auto executor{
        [&visualization_actor](FMassExecutionContext& context, auto& Data, uint32 EntityIndex) {
            auto const transforms{context.GetFragmentView<FMassBulletTransformFragment>()};
            auto const indices{context.GetFragmentView<FMassBulletInstanceIndexFragment>()};

            visualization_actor->update_instance(indices[EntityIndex].instance_index,
                                                 transforms[EntityIndex].transform);
        }};

    ForEachEntity(context, accessors, std::move(executor));
}

UMassBulletVisualizationProcessor::UMassBulletVisualizationProcessor()
    : entity_query(*this) {
    executor =
        UE::Mass::FQueryExecutor::CreateQuery<FMassBulletVisualizationExecutor>(entity_query, this);
    AutoExecuteQuery = executor;

    if (HasAnyFlags(RF_ClassDefaultObject)) {
        SetShouldAutoRegisterWithGlobalList(true);
        ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::SyncWorldToMass;
        bRequiresGameThreadExecution = true;
        set_execution_flags(EProcessorExecutionFlags::All);
    }
}
