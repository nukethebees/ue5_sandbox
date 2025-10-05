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
            TEXT("Sandbox::FMassBulletCollisionExecutor::Execute::executor"))
        auto const transforms{context.GetFragmentView<FMassBulletTransformFragment>()};
        auto const velocities{context.GetFragmentView<FMassBulletVelocityFragment>()};
        auto const last_positions{
            context.GetMutableFragmentView<FMassBulletLastPositionFragment>()};
        auto const hit_infos{context.GetMutableFragmentView<FMassBulletHitInfoFragment>()};

        auto const current_position{transforms[i].transform.GetLocation()};
        auto const last_position{last_positions[i].last_position};

        TRY_INIT_PTR(world, context.GetWorld());

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

        auto& command_buffer{context.Defer()};

        if (hit_detected && hit_results.Num() > 0) {
            // hit_infos[i].hit_location = hit_results[0].Location;
            hit_infos[i].hit_location = hit_results[0].ImpactPoint;
            // hit_infos[i].hit_normal = hit_results[0].Normal;
            hit_infos[i].hit_normal = hit_results[0].ImpactNormal;
            // hit_infos[i].hit_normal = transforms[i].transform.Rotator().Vector();

            auto const reflection_vector{FMath::GetReflectionVector(
                velocities[i].velocity.GetSafeNormal(), hit_results[0].ImpactNormal)};
            hit_infos[i].hit_normal = reflection_vector;

            command_buffer.AddTag<FMassBulletDeadTag>(context.GetEntity(i));
            command_buffer.RemoveTag<FMassBulletActiveTag>(context.GetEntity(i));
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
