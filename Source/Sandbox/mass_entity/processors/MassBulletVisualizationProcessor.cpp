#include "Sandbox/mass_entity/processors/MassBulletVisualizationProcessor.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassProcessingTypes.h"

#include "Sandbox/actor_components/MassBulletVisualizationComponent.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

#include "Sandbox/macros/null_checks.hpp"

void FMassBulletVisualizationExecutor::Execute(FMassExecutionContext& context) {
    auto executor{[this](FMassExecutionContext& context, auto& Data, uint32 EntityIndex) {
        auto const visualization_fragment{
            context.GetFragmentView<FMassBulletVisualizationComponentFragment>()};

        auto const transforms{context.GetFragmentView<FMassBulletTransformFragment>()};
        auto const indices{context.GetFragmentView<FMassBulletInstanceIndexFragment>()};

        auto const n{context.GetNumEntities()};
        visualization_fragment[EntityIndex].component->update_instance(
            indices[EntityIndex].instance_index, transforms[EntityIndex].transform);
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
