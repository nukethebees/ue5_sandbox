#include "Sandbox/environment/effects/actors/DayNightCycle.h"

ADayNightCycle::ADayNightCycle() {
    PrimaryActorTick.bCanEverTick = true;
}

void ADayNightCycle::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    if (sun_light) {
        FRotator delta_rotation(0.0f, rotation_speed_degrees_per_second * DeltaTime, 0.0f);
        sun_light->AddActorLocalRotation(delta_rotation);
    }
}
