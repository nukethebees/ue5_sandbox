#include "Sandbox/mass_entity/processors/MassBulletCollisionProcessor.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

void FMassBulletCollisionExecutor::Execute(FMassExecutionContext& context) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletCollisionExecutor::Execute"))

    constexpr auto executor{[](FMassExecutionContext& context, auto& Data, uint32 EntityIndex) {
        auto const transforms{context.GetFragmentView<FMassBulletTransformFragment>()};
        auto const last_positions{
            context.GetMutableFragmentView<FMassBulletLastPositionFragment>()};
        auto const hit_infos{context.GetMutableFragmentView<FMassBulletHitInfoFragment>()};

        auto const current_position{transforms[EntityIndex].transform.GetLocation()};
        auto const last_position{last_positions[EntityIndex].last_position};

        auto* world{context.GetWorld()};
        if (!world) {
            return;
        }

        FCollisionQueryParams query_params{};
        query_params.bTraceComplex = false;
        query_params.bReturnPhysicalMaterial = false;

        static constexpr auto collision_shape_radius{2.0f};
        FCollisionShape collision_shape{};
        collision_shape.SetSphere(collision_shape_radius);

        TArray<FHitResult> hit_results{};
        auto const hit_detected{world->SweepMultiByChannel(hit_results,
                                                           last_position,
                                                           current_position,
                                                           FQuat::Identity,
                                                           ECC_GameTraceChannel1,
                                                           collision_shape,
                                                           query_params)};

        if (hit_detected && hit_results.Num() > 0) {
            hit_infos[EntityIndex].hit_location = hit_results[0].Location;
            hit_infos[EntityIndex].hit_normal = hit_results[0].Normal;
            context.Defer().AddTag<FMassBulletDeadTag>(context.GetEntity(EntityIndex));
        }

        last_positions[EntityIndex].last_position = current_position;
    }};

    ForEachEntity(context, accessors, std::move(executor));
}

UMassBulletCollisionProcessor::UMassBulletCollisionProcessor()
    : entity_query(*this)
    , executor(
          UE::Mass::FQueryExecutor::CreateQuery<FMassBulletCollisionExecutor>(entity_query, this)) {
    AutoExecuteQuery = executor;
    SetProcessingPhase(EMassProcessingPhase::EndPhysics);

    if (HasAnyFlags(RF_ClassDefaultObject)) {
        bRequiresGameThreadExecution = true;
        set_execution_flags(EProcessorExecutionFlags::All);
    }
}
