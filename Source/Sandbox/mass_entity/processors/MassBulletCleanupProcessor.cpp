#include "Sandbox/mass_entity/processors/MassBulletCleanupProcessor.h"

#include "Kismet/KismetMathLibrary.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "NiagaraFunctionLibrary.h"

#include "Sandbox/actors/MassBulletVisualizationActor.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

#include "Sandbox/macros/null_checks.hpp"

void FMassBulletCleanupExecutor::Execute(FMassExecutionContext& context) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletCleanupExecutor::Execute"))

    constexpr auto executor{[](FMassExecutionContext& context, auto& Data, uint32 EntityIndex) {
        auto const viz_fragment{
            context.GetConstSharedFragment<FMassBulletVisualizationActorFragment>()};
        RETURN_IF_NULLPTR(viz_fragment.actor);

        auto const indices{context.GetFragmentView<FMassBulletInstanceIndexFragment>()};
        auto const hit_infos{context.GetFragmentView<FMassBulletHitInfoFragment>()};

        auto const instance_index{indices[EntityIndex].instance_index};

        auto impact_effect_fragment{
            context.GetConstSharedFragment<FMassBulletImpactEffectFragment>()};

        if (impact_effect_fragment.impact_effect) {
            auto const impact_location{hit_infos[EntityIndex].hit_location};
            auto const impact_rotation{
                UKismetMathLibrary::MakeRotFromZ(hit_infos[EntityIndex].hit_normal)};

            UNiagaraFunctionLibrary::SpawnSystemAtLocation(context.GetWorld(),
                                                           impact_effect_fragment.impact_effect,
                                                           impact_location,
                                                           impact_rotation);
        }

        viz_fragment.actor->remove_instance(instance_index);

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
