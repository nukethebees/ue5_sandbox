#include "Sandbox/mass_entity/processors/MassBulletCollisionProcessor.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

#include "Sandbox/macros/null_checks.hpp"

void FMassBulletCollisionExecutor::Execute(FMassExecutionContext& context) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::FMassBulletCollisionExecutor::Execute"))

    TRY_INIT_PTR(world, context.GetWorld());

    FCollisionQueryParams query_params{};
    query_params.bTraceComplex = false;
    query_params.bReturnPhysicalMaterial = false;

    FCollisionShape collision_shape{};
    collision_shape.SetSphere(collision_shape_radius);

    auto executor{
        [world, &query_params, &collision_shape](FMassExecutionContext& context, auto& Data) {
            TRACE_CPUPROFILER_EVENT_SCOPE(
                TEXT("Sandbox::FMassBulletCollisionExecutor::Execute::lambda"))
            auto const n{context.GetNumEntities()};
            auto const transforms{context.GetMutableFragmentView<FMassBulletTransformFragment>()};
            auto const velocities{context.GetFragmentView<FMassBulletVelocityFragment>()};
            auto const last_positions{
                context.GetMutableFragmentView<FMassBulletLastPositionFragment>()};
            auto const hit_infos{context.GetMutableFragmentView<FMassBulletHitInfoFragment>()};
            auto const state_fragments{context.GetMutableFragmentView<FMassBulletStateFragment>()};

            for (int32 i{0}; i < n; ++i) {
                if (state_fragments[i].state != EBulletState::Active) {
                    continue;
                }

                auto const current_position{transforms[i].transform.GetLocation()};
                auto const last_position{last_positions[i].last_position};

                FHitResult hit_result{};
                auto const hit_detected{world->SweepSingleByChannel(hit_result,
                                                                    last_position,
                                                                    current_position,
                                                                    FQuat::Identity,
                                                                    ECC_GameTraceChannel1,
                                                                    collision_shape,
                                                                    query_params)};

                if (hit_detected) {
                    hit_infos[i].hit_location = hit_result.ImpactPoint;
                    hit_infos[i].hit_normal = FMath::GetReflectionVector(
                        velocities[i].velocity.GetSafeNormal(), hit_result.ImpactNormal);

                    state_fragments[i].state = EBulletState::Hit;

                    // Move bullet back to impact point to prevent tunneling through walls
                    transforms[i].transform.SetLocation(hit_result.ImpactPoint);
                    last_positions[i].last_position = hit_result.ImpactPoint;
                } else {
                    last_positions[i].last_position = current_position;
                }
            }
        }};

    ParallelForEachEntityChunk(context, accessors, std::move(executor));
}

UMassBulletCollisionProcessor::UMassBulletCollisionProcessor()
    : entity_query(*this)
    , executor(
          UE::Mass::FQueryExecutor::CreateQuery<FMassBulletCollisionExecutor>(entity_query, this)) {
    AutoExecuteQuery = executor;
    SetProcessingPhase(EMassProcessingPhase::EndPhysics);

    if (HasAnyFlags(RF_ClassDefaultObject)) {
        set_execution_flags(EProcessorExecutionFlags::All);
    }
}
