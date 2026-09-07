#include "SpaceGame/ships/common/SpaceShipControllerInputs.h"

#include "InputMappingContext.h"

auto FSpaceShipControllerInputs::get_mapping_context() const -> UInputMappingContext* {
    if (IsValid(mapping_context)) {
        return mapping_context;
    }
    for (auto* const legacy_context : mapping_contexts_DEPRECATED) {
        if (IsValid(legacy_context) &&
            !legacy_context->GetProfilesWithOverridenMappings().IsEmpty()) {
            return legacy_context;
        }
    }
    if (mapping_contexts_DEPRECATED.IsValidIndex(initial_mapping_context_index_DEPRECATED)) {
        return mapping_contexts_DEPRECATED[initial_mapping_context_index_DEPRECATED];
    }
    return nullptr;
}
