#include "Sandbox/mass_entity/processors/MassBulletCleanupProcessor.h"

#include "Kismet/KismetMathLibrary.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

#include "Sandbox/actors/MassBulletVisualizationActor.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/subsystems/world/BulletSparkEffectSubsystem.h"
#include "Sandbox/subsystems/world/MassBulletSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

void FMassBulletCleanupExecutor::Execute(FMassExecutionContext& context) {
    constexpr auto logger{NestedLogger<"Execute">()};

    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletCleanupExecutor::Execute"))

    TRY_INIT_PTR(world, context.GetWorld());
    TRY_INIT_PTR(mass_bullet_subsystem, world->GetSubsystem<UMassBulletSubsystem>());
    TRY_INIT_PTR(spark_effect_subsystem, world->GetSubsystem<UBulletSparkEffectSubsystem>());

    auto executor{[mass_bullet_subsystem, spark_effect_subsystem](FMassExecutionContext& context,
                                                                  auto& Data) {
        auto const n{context.GetNumEntities()};

        auto const hit_occurred_flags{
            context.GetMutableFragmentView<FMassBulletHitOccurredFragment>()};
        auto const viz_fragment{
            context.GetConstSharedFragment<FMassBulletVisualizationActorFragment>()};
        RETURN_IF_NULLPTR(viz_fragment.actor);

        auto const indices{context.GetFragmentView<FMassBulletInstanceIndexFragment>()};
        auto const hit_infos{context.GetFragmentView<FMassBulletHitInfoFragment>()};

        auto impact_effect_fragment{
            context.GetConstSharedFragment<FMassBulletImpactEffectFragment>()};

        for (int32 i{0}; i < n; ++i) {
            if (!hit_occurred_flags[i].hit_occurred) {
                continue;
            }

            auto const instance_index{indices[i].instance_index};

            if (impact_effect_fragment.impact_effect) {
                auto const impact_location{hit_infos[i].hit_location};
                auto const impact_rotation{hit_infos[i].hit_normal};
                spark_effect_subsystem->add_impact(impact_location, impact_rotation);
            }

            viz_fragment.actor->remove_instance(instance_index);

            auto entity{context.GetEntity(i)};

            hit_occurred_flags[i].hit_occurred = false;
            mass_bullet_subsystem->return_bullet(entity);
        }
    }};

    ForEachEntityChunk(context, accessors, std::move(executor));
}

UMassBulletCleanupProcessor::UMassBulletCleanupProcessor()
    : entity_query(*this) {
    executor =
        UE::Mass::FQueryExecutor::CreateQuery<FMassBulletCleanupExecutor>(entity_query, this);
    AutoExecuteQuery = executor;

    SetProcessingPhase(EMassProcessingPhase::FrameEnd);

    if (HasAnyFlags(RF_ClassDefaultObject)) {
        bRequiresGameThreadExecution = true;
        set_execution_flags(EProcessorExecutionFlags::All);
    }
}
