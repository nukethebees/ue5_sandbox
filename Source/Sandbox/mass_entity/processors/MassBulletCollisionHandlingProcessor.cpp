#include "Sandbox/mass_entity/processors/MassBulletCollisionHandlingProcessor.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/subsystems/world/BulletSparkEffectSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

void FMassBulletCollisionHandlingExecutor::Execute(FMassExecutionContext& context) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletCollisionHandlingExecutor::Execute"))
    constexpr auto logger{NestedLogger<"Execute">()};

    TRY_INIT_PTR(world, context.GetWorld());
    TRY_INIT_PTR(spark_effect_subsystem, world->GetSubsystem<UBulletSparkEffectSubsystem>());

    auto executor{[spark_effect_subsystem](FMassExecutionContext& context, auto& Data) {
        auto const n{context.GetNumEntities()};
        auto const state_fragments{context.GetFragmentView<FMassBulletStateFragment>()};
        auto const hit_infos{context.GetFragmentView<FMassBulletHitInfoFragment>()};
        auto const impact_effect_fragment{
            context.GetConstSharedFragment<FMassBulletImpactEffectFragment>()};

        for (int32 i{0}; i < n; ++i) {
            if (!state_fragments[i].hit_occurred) {
                continue;
            }

            if (impact_effect_fragment.impact_effect) {
                auto const impact_location{hit_infos[i].hit_location};
                auto const impact_rotation{hit_infos[i].hit_normal};
                spark_effect_subsystem->add_impact(impact_location, impact_rotation);
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

    SetProcessingPhase(EMassProcessingPhase::FrameEnd);
    ExecutionOrder.ExecuteAfter.Add(ml::ProcessorGroupNames::CollisionDetection);
    ExecutionOrder.ExecuteInGroup = ml::ProcessorGroupNames::CollisionHandling;

    if (HasAnyFlags(RF_ClassDefaultObject)) {
        bRequiresGameThreadExecution = true;
        set_execution_flags(EProcessorExecutionFlags::All);
    }
}
