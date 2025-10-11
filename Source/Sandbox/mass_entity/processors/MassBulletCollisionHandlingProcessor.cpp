#include "Sandbox/mass_entity/processors/MassBulletCollisionHandlingProcessor.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

#include "Sandbox/actor_components/HealthComponent.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/mass_entity/processors/BulletProcessorGroups.h"
#include "Sandbox/subsystems/world/BulletSparkEffectSubsystem.h"
#include "Sandbox/subsystems/world/DamageManagerSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

void FMassBulletCollisionHandlingExecutor::Execute(FMassExecutionContext& context) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletCollisionHandlingExecutor::Execute"))
    constexpr auto logger{NestedLogger<"Execute">()};

    TRY_INIT_PTR(world, context.GetWorld());
    TRY_INIT_PTR(spark_effect_subsystem, world->GetSubsystem<UBulletSparkEffectSubsystem>());
    TRY_INIT_PTR(damage_manager, world->GetSubsystem<UDamageManagerSubsystem>());

    auto executor{[spark_effect_subsystem, damage_manager](FMassExecutionContext& context,
                                                           auto& Data) {
        auto const n{context.GetNumEntities()};
        auto const state_fragments{context.GetFragmentView<FMassBulletStateFragment>()};
        auto const hit_infos{context.GetFragmentView<FMassBulletHitInfoFragment>()};
        auto const impact_effect_fragment{
            context.GetConstSharedFragment<FMassBulletImpactEffectFragment>()};
        auto const damage_fragment{context.GetConstSharedFragment<FMassBulletDamageFragment>()};

        RETURN_IF_NULLPTR(impact_effect_fragment.impact_effect);

        for (int32 i{0}; i < n; ++i) {
            if (!state_fragments[i].hit_occurred) {
                continue;
            }
            spark_effect_subsystem->add_impact(hit_infos[i].hit_location, hit_infos[i].hit_normal);

            if (auto* hit_actor{hit_infos[i].hit_actor.Get()}) {
                if (auto* health_component{hit_actor->FindComponentByClass<UHealthComponent>()}) {
                    damage_manager->queue_health_change(health_component, damage_fragment.damage);
                }
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
