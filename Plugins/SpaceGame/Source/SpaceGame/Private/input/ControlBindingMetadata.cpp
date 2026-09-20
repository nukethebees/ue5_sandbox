#include "SpaceGame/input/ControlBindingMetadata.h"

namespace ml::ioj {

auto control_binding_group_label(EControlBindingGroup const group) -> FText {
    switch (group) {
        case EControlBindingGroup::Flight:
            return NSLOCTEXT("Controls", "FlightCategory", "Flight");
        case EControlBindingGroup::Combat:
            return NSLOCTEXT("Controls", "CombatCategory", "Combat");
        case EControlBindingGroup::Utility:
            return NSLOCTEXT("Controls", "UtilityCategory", "Utility");
    }
    return FText::GetEmpty();
}

} // namespace ml::ioj
