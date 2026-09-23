#pragma once

#include <ioj/sim/player/flight_model_config.h>

#include <CoreMinimal.h>

namespace ml::ioj::flight_model_settings_codec {
SPACEGAME_API auto encode(::ioj::sim::player::FlightModelProfile const& profile) -> FString;
SPACEGAME_API auto decode(FString const& data, ::ioj::sim::player::FlightModelProfile& profile)
    -> bool;
}
