#include "Sandbox/actors/DayNightCycle.h"

ADayNightCycle::ADayNightCycle() {
    PrimaryActorTick.bCanEverTick = true;
}

void ADayNightCycle::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    if (sun_light) {
        auto const current_rotation{sun_light->GetActorRotation()};
        auto const delta_pitch{rotation_speed_degrees_per_second * DeltaTime};

        sun_light->SetActorRotation(FRotator(
            current_rotation.Pitch + delta_pitch, current_rotation.Yaw, current_rotation.Roll));
    }
}
