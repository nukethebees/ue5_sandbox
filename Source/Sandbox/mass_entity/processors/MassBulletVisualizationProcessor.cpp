#include "Sandbox/mass_entity/processors/MassBulletVisualizationProcessor.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassProcessingTypes.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

#include "Sandbox/macros/null_checks.hpp"

void FMassBulletVisualizationExecutor::Execute(FMassExecutionContext& context) {
    auto executor{[this](FMassExecutionContext& context, auto& Data, uint32 EntityIndex) {
        RETURN_IF_NULLPTR(visualization_component);

        auto const transforms{context.GetFragmentView<FMassBulletTransformFragment>()};
        auto const indices{context.GetFragmentView<FMassBulletInstanceIndexFragment>()};

        auto const n{context.GetNumEntities()};
        for (auto i{0}; i < n; ++i) {
            visualization_component->update_instance(indices[i].instance_index,
                                                     transforms[i].transform);
        }
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
        set_execution_flags(EProcessorExecutionFlags::All);
    }
}

void UMassBulletVisualizationProcessor::set_visualization_component(
    UMassBulletVisualizationComponent* component) {
    visualization_component = component;
    if (executor.IsValid()) {
        executor->visualization_component = component;
    }
}
