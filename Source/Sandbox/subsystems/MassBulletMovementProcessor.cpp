// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/subsystems/MassBulletMovementProcessor.h"

#include "MassExecutionContext.h"

#include "Sandbox/data/mass/MassBulletFragments.h"

UMassBulletMovementProcessor::UMassBulletMovementProcessor() {
    // ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
    ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames)
}

void UMassBulletMovementProcessor::ConfigureQueries(TSharedRef<FMassEntityManager> const& manager) {
    entity_query.AddRequirement<FMassBulletTransformFragment>(EMassFragmentAccess::ReadWrite);
    entity_query.AddRequirement<FMassBulletVelocityFragment>(EMassFragmentAccess::ReadOnly);
}

void UMassBulletMovementProcessor::Execute(FMassEntityManager& EntityManager,
                                           FMassExecutionContext& Context) {
    entity_query.ForEachEntityChunk(Context, [this](FMassExecutionContext& Context) {
        auto const delta_time{Context.GetDeltaTimeSeconds()};
        auto const transforms{Context.GetMutableFragmentView<FMassBulletTransformFragment>()};
        auto const velocities{Context.GetFragmentView<FMassBulletVelocityFragment>()};

        for (auto i{0}; i < Context.GetNumEntities(); ++i) {
            auto const displacement{velocities[i].velocity * delta_time};
            transforms[i].transform.AddToTranslation(displacement);
        }
    });
}
