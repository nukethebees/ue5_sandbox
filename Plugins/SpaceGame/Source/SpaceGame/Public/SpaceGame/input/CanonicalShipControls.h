#pragma once

#include "SpaceGame/input/ControlBindingMetadata.h"

class UInputMappingContext;

namespace ioj {
struct FCanonicalShipControlContext {
    EShipControlScope scope;
    TCHAR const* asset_name;
    TCHAR const* package_path;
};

SPACEGAME_API auto canonical_ship_control_contexts()
    -> TConstArrayView<FCanonicalShipControlContext>;
SPACEGAME_API auto canonical_ship_control_context(EShipControlScope scope)
    -> FCanonicalShipControlContext const&;
SPACEGAME_API auto load_ship_control_context(EShipControlScope scope) -> UInputMappingContext*;
} // namespace ioj
