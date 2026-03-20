#pragma once

#include "Sandbox/players/SpeedResponse.h"

#include "SpaceShipFlightModel.generated.h"

USTRUCT(BlueprintType)
struct FSpaceShipFlightModel {
    GENERATED_BODY()

    FSpaceShipFlightModel() = default;
    FSpaceShipFlightModel(FSpeedResponse sr)
        : response(sr) {}

    auto step_size() const { return target_speed - old_speed; }

    auto calculate_dy(float t) const -> float;
    auto update_y(float dt) -> float;
    void set_new_impulse(FSpeedResponse sr, float old_s, float target_s);
  protected:
    UPROPERTY(VisibleAnywhere)
    FSpeedResponse response{};
    UPROPERTY(VisibleAnywhere)
    float time{0.f};
    UPROPERTY(VisibleAnywhere)
    float old_speed{0.f};
    UPROPERTY(VisibleAnywhere)
    float target_speed{0.f};
    UPROPERTY(VisibleAnywhere)
    float alpha{0.f};
    UPROPERTY(VisibleAnywhere)
    float wn{0.f};
    UPROPERTY(VisibleAnywhere)
    float wd{0.f};
    UPROPERTY(VisibleAnywhere)
    float z_wn{0.f};
    // Harmonic addition theorem
    // sin(x) + b*cos(x) -> c * cos(x + delta)
    UPROPERTY(VisibleAnywhere)
    float delta{0.f};
    UPROPERTY(VisibleAnywhere)
    float c{0.f};

#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, Category = "Debug")
    mutable float h_dbg{0.f};
    UPROPERTY(VisibleAnywhere, Category = "Debug")
    float step_size_original_dbg{0.f};
    UPROPERTY(VisibleAnywhere, Category = "Debug")
    float step_size_dbg{0.f};
    UPROPERTY(VisibleAnywhere, Category = "Debug")
    float dy_dbg{0.f};
#endif
};
