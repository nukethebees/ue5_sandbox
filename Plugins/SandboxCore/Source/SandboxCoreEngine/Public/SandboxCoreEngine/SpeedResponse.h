#pragma once

#include "CoreMinimal.h"

#include <sandbox/core/speed_response.h>

#include "SpeedResponse.generated.h"

USTRUCT(BlueprintType)
struct FSpeedResponse {
    GENERATED_BODY()

    FSpeedResponse() = default;
    FSpeedResponse(float settling_time, float damping_ratio)
        : settling_time(settling_time)
        , damping_ratio(damping_ratio) {}

    auto tau() const { return settling_time / 5.f; }
    auto natural_angular_frequency() const { return 1 / tau(); }

    UPROPERTY(EditAnywhere)
    float settling_time{3.f};
    UPROPERTY(EditAnywhere)
    float damping_ratio{0.5f};
};

USTRUCT(BlueprintType)
struct FSpeedResponses {
    GENERATED_BODY()

    FSpeedResponses() = default;

    UPROPERTY(EditAnywhere)
    FSpeedResponse boost{};
    UPROPERTY(EditAnywhere)
    FSpeedResponse brake{};
    UPROPERTY(EditAnywhere)
    FSpeedResponse slowing_to_cruise{};
    UPROPERTY(EditAnywhere)
    FSpeedResponse accelerating_to_cruise{};
};

namespace ml {
inline auto to_native(FSpeedResponse const value) noexcept -> SpeedResponse {
    return {.settling_time = value.settling_time, .damping_ratio = value.damping_ratio};
}

inline auto to_native(FSpeedResponses const& value) noexcept -> SpeedResponses {
    return {.boost = to_native(value.boost),
            .brake = to_native(value.brake),
            .slowing_to_cruise = to_native(value.slowing_to_cruise),
            .accelerating_to_cruise = to_native(value.accelerating_to_cruise)};
}
}
