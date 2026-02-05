#include "Sandbox/combat/bullets/MassBulletCollisionHandlingProcessor.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

#include "Sandbox/combat/bullets/BulletProcessorGroups.h"
#include "Sandbox/combat/bullets/MassBulletFragments.h"
#include "Sandbox/environment/effects/NiagaraNdcWriterSubsystem.h"
#include "Sandbox/health/DamageManagerSubsystem.h"
#include "Sandbox/health/HealthComponent.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void FMassBulletCollisionHandlingExecutor::Execute(FMassExecutionContext& context) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletCollisionHandlingExecutor::Execute"))
    constexpr auto logger{NestedLogger<"Execute">()};

    TRY_INIT_PTR(world, context.GetWorld());
    TRY_INIT_PTR(ndc_subsystem_ptr, world->GetSubsystem<UNiagaraNdcWriterSubsystem>());
    TRY_INIT_PTR(damage_manager_ptr, world->GetSubsystem<UDamageManagerSubsystem>());

    auto executor{[&damage_manager = *damage_manager_ptr, &ndc_subsystem = *ndc_subsystem_ptr](
                      FMassExecutionContext& context, auto& Data) {
        auto const n{context.GetNumEntities()};
        auto const state_fragments{context.GetFragmentView<FMassBulletStateFragment>()};
        auto const hit_infos{context.GetFragmentView<FMassBulletHitInfoFragment>()};
        auto const& impact_effect_fragment{
            context.GetConstSharedFragment<FMassBulletImpactEffectFragment>()};
        auto const& damage_fragments{context.GetFragmentView<FMassBulletDamageFragment>()};

        for (int32 i{0}; i < n; ++i) {
            if (!state_fragments[i].hit_occurred) {
                continue;
            }
            ndc_subsystem.add_payload(impact_effect_fragment.effect_index,
                                      hit_infos[i].hit_location,
                                      hit_infos[i].hit_normal);

            if (auto* hit_actor{hit_infos[i].hit_actor.Get()}) {
                if (auto* health_component{hit_actor->FindComponentByClass<UHealthComponent>()}) {
                    damage_manager.queue_health_change(health_component,
                                                       damage_fragments[i].damage);
                }
            } else {
                logger.log_warning(TEXT("hit_actor is null"));
            }
        }
    }};

    ForEachEntityChunk(context, accessors, std::move(executor));
}

UMassBulletCollisionHandlingProcessor::UMassBulletCollisionHandlingProcessor()
    : entity_query(*this) {
    executor = UE::Mass::FQueryExecutor::CreateQuery<FMassBulletCollisionHandlingExecutor>(
        entity_query, this);
    AutoExecuteQuery = executor;

    SetProcessingPhase(EMassProcessingPhase::PrePhysics);
    ExecutionOrder.ExecuteAfter.Add(ml::ProcessorGroupNames::CollisionDetection);
    ExecutionOrder.ExecuteInGroup = ml::ProcessorGroupNames::CollisionHandling;

    if (HasAnyFlags(RF_ClassDefaultObject)) {
        set_execution_flags(EProcessorExecutionFlags::All);
    }
}
