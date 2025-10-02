#include "Sandbox/mass_entity/processors/MassBulletVisualizationProcessor.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

UMassBulletVisualizationProcessor::UMassBulletVisualizationProcessor() {
    executor =
        UE::Mass::FQueryExecutor::CreateQuery<FMassBulletVisualizationExecutor>(entity_query, this);
}

void UMassBulletVisualizationProcessor::set_visualization_component(
    UMassBulletVisualizationComponent* component) {
    visualization_component = component;
    if (executor.IsValid()) {
        executor->visualization_component = component;
    }
}
