#include "TestCollisionActor.h"

#include <Components/BoxComponent.h>

namespace {
void configure_collision(AActor& actor, UBoxComponent& component) {
    actor.SetRootComponent(&component);
    component.SetMobility(EComponentMobility::Static);
    component.SetBoxExtent(FVector{100.f, 1000.f, 1000.f});
    component.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    component.SetCollisionObjectType(ECC_WorldStatic);
    component.SetCollisionResponseToAllChannels(ECR_Ignore);
    component.SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}
}

ASandboxTestCollisionActor::ASandboxTestCollisionActor()
    : collision_component{CreateDefaultSubobject<UBoxComponent>(TEXT("collision"))} {
    configure_collision(*this, *collision_component);
}

ASandboxTestOmittedCollisionActor::ASandboxTestOmittedCollisionActor()
    : collision_component{CreateDefaultSubobject<UBoxComponent>(TEXT("collision"))} {
    configure_collision(*this, *collision_component);
}
