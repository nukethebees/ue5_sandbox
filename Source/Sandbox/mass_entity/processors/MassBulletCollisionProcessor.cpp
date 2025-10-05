#include "Sandbox/mass_entity/processors/MassBulletCollisionProcessor.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

#include "Sandbox/macros/null_checks.hpp"

void FMassBulletCollisionExecutor::Execute(FMassExecutionContext& context) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletCollisionExecutor::Execute"))

    constexpr auto executor{[](FMassExecutionContext& context, auto& Data, uint32 i) {
        TRACE_CPUPROFILER_EVENT_SCOPE(
            TEXT("Sandbox::FMassBulletCollisionExecutor::Execute[executor::get_views]"))
        auto const transforms{context.GetFragmentView<FMassBulletTransformFragment>()};
        auto const velocities{context.GetFragmentView<FMassBulletVelocityFragment>()};
        auto const last_positions{
            context.GetMutableFragmentView<FMassBulletLastPositionFragment>()};
        auto const hit_infos{context.GetMutableFragmentView<FMassBulletHitInfoFragment>()};
        auto const hit_occurred_flags{
            context.GetMutableFragmentView<FMassBulletHitOccurredFragment>()};

        auto const current_position{transforms[i].transform.GetLocation()};
        auto const last_position{last_positions[i].last_position};

        TRY_INIT_PTR(world, context.GetWorld());

        TRACE_CPUPROFILER_EVENT_SCOPE(
            TEXT("Sandbox::FMassBulletCollisionExecutor::Execute[executor::do_trace]"))
        FCollisionQueryParams query_params{};
        query_params.bTraceComplex = false;
        query_params.bReturnPhysicalMaterial = false;

        static constexpr auto collision_shape_radius{2.0f};
        FCollisionShape collision_shape{};
        collision_shape.SetSphere(collision_shape_radius);

        FHitResult hit_result{};
        auto const hit_detected{world->SweepSingleByChannel(hit_result,
                                                            last_position,
                                                            current_position,
                                                            FQuat::Identity,
                                                            ECC_GameTraceChannel1,
                                                            collision_shape,
                                                            query_params)};

        TRACE_CPUPROFILER_EVENT_SCOPE(
            TEXT("Sandbox::FMassBulletCollisionExecutor::Execute[executor::process_trace_result]"))
        if (hit_detected) {
            hit_infos[i].hit_location = hit_result.ImpactPoint;
            hit_infos[i].hit_normal = FMath::GetReflectionVector(
                velocities[i].velocity.GetSafeNormal(), hit_result.ImpactNormal);

            hit_occurred_flags[i].hit_occurred = true;
        }

        last_positions[i].last_position = current_position;
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
