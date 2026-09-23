#pragma once

#include "SpaceGame/input/ControlBindingMetadata.h"

#include <ioj/sim/player/flight_model_config.h>

class UInputMappingContext;

namespace ml::ioj {
struct FCanonicalShipControlContext {
    EShipControlScope scope;
    TCHAR const* asset_name;
    TCHAR const* package_path;
};

SPACEGAME_API auto canonical_ship_control_contexts()
    -> TConstArrayView<FCanonicalShipControlContext>;
SPACEGAME_API auto canonical_ship_control_context(EShipControlScope scope)
    -> FCanonicalShipControlContext const&;
SPACEGAME_API auto flight_control_scope(::ioj::sim::player::FlightModelSlot slot)
    -> EShipControlScope;
SPACEGAME_API auto flight_model_slot(EShipControlScope scope)
    -> ::ioj::sim::player::FlightModelSlot;
SPACEGAME_API auto load_ship_control_context(EShipControlScope scope) -> UInputMappingContext*;
} // namespace ml::ioj
