// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/subsystems/MassBulletVisualizationProcessor.h"

#include "Sandbox/data/mass/MassBulletFragments.h"

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
