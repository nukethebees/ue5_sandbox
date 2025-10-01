// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/subsystems/MassBulletVisualizationProcessor.h"

#include "MassExecutionContext.h"

#include "Sandbox/actor_components/MassBulletVisualizationComponent.h"
#include "Sandbox/data/mass/MassBulletFragments.h"

UMassBulletVisualizationProcessor::UMassBulletVisualizationProcessor() {
    // ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;
    // ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Movement);
}

void UMassBulletVisualizationProcessor::set_visualization_component(
    UMassBulletVisualizationComponent* component) {
    visualization_component = component;
}

void UMassBulletVisualizationProcessor::ConfigureQueries(
    TSharedRef<FMassEntityManager> const& manager) {
    entity_query.AddRequirement<FMassBulletTransformFragment>(EMassFragmentAccess::ReadOnly);
    entity_query.AddRequirement<FMassBulletInstanceIndexFragment>(EMassFragmentAccess::ReadOnly);
}

void UMassBulletVisualizationProcessor::Execute(FMassEntityManager& EntityManager,
                                                FMassExecutionContext& Context) {
    if (!visualization_component) {
        return;
    }

    entity_query.ForEachEntityChunk(Context, [this](FMassExecutionContext& Context) {
        auto const transforms{Context.GetFragmentView<FMassBulletTransformFragment>()};
        auto const indices{Context.GetFragmentView<FMassBulletInstanceIndexFragment>()};

        for (auto i{0}; i < Context.GetNumEntities(); ++i) {
            visualization_component->update_instance(indices[i].instance_index,
                                                     transforms[i].transform);
        }
    });
}
