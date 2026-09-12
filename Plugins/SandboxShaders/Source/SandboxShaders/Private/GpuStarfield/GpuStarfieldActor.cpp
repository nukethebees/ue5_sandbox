#include "SandboxShaders/GpuStarfield/GpuStarfieldActor.h"

AGpuStarfieldActor::AGpuStarfieldActor() {
    PrimaryActorTick.bCanEverTick = false;

    starfield_component_ = CreateDefaultSubobject<UGpuStarfieldComponent>(TEXT("GpuStarfield"));
    SetRootComponent(starfield_component_);
}

void AGpuStarfieldActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    apply_settings();
}

void AGpuStarfieldActor::PostRegisterAllComponents() {
    Super::PostRegisterAllComponents();
    apply_settings();
}

void AGpuStarfieldActor::apply_settings() {
    if (IsValid(starfield_component_)) {
        starfield_component_->apply_settings(settings);
    }
}
