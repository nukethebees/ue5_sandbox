#include "Sandbox/mass_entity/processors/MassBulletCleanupProcessor.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/actor_components/MassBulletVisualizationComponent.h"

#include "Sandbox/macros/null_checks.hpp"

void FMassBulletCleanupExecutor::Execute(FMassExecutionContext& context) {
    auto executor{[this](FMassExecutionContext& context, auto& Data, uint32 EntityIndex) {
        auto const indices{context.GetFragmentView<FMassBulletInstanceIndexFragment>()};
        auto const visualization_components{
            context.GetFragmentView<FMassBulletVisualizationComponentFragment>()};

        auto const instance_index{indices[EntityIndex].instance_index};
        auto viz_component{visualization_components[EntityIndex].component};

        if (viz_component) {
            viz_component->remove_instance(instance_index);
        }

        context.Defer().DestroyEntity(context.GetEntity(EntityIndex));
    }};

    ForEachEntity(context, accessors, std::move(executor));
}

UMassBulletCleanupProcessor::UMassBulletCleanupProcessor()
    : entity_query(*this) {
    executor =
        UE::Mass::FQueryExecutor::CreateQuery<FMassBulletCleanupExecutor>(entity_query, this);
    AutoExecuteQuery = executor;

    if (HasAnyFlags(RF_ClassDefaultObject)) {
        SetShouldAutoRegisterWithGlobalList(true);
        ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::ApplyForces);
        bRequiresGameThreadExecution = true;
        set_execution_flags(EProcessorExecutionFlags::All);
    }
}
