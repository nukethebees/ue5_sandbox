#include "SpaceGame/input/ControlBindingMetadata.h"

namespace ioj {

auto control_binding_group_label(BindingGroup const group) -> FText {
    switch (group) {
        case BindingGroup::Flight:
            return NSLOCTEXT("Controls", "FlightCategory", "Flight");
        case BindingGroup::Combat:
            return NSLOCTEXT("Controls", "CombatCategory", "Combat");
        case BindingGroup::Utility:
            return NSLOCTEXT("Controls", "UtilityCategory", "Utility");
    }
    return FText::GetEmpty();
}

} // namespace ioj
