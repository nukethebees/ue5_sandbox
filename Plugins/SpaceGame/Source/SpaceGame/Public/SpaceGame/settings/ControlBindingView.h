#pragma once

#include <SpaceGame/input/ControlBindingMetadata.h>
#include <SpaceGame/settings/ControlChordBindingView.h>

namespace ioj {
struct FControlBindingView {
    auto is_modified() const -> bool { return current_key != default_key; }

    FControlBindingAddress address;
    FText display_name;
    FText display_category;
    EControlBindingGroup display_group{EControlBindingGroup::Flight};
    int32 display_order{};
    EShipControlScope scope{EShipControlScope::General};
    EHardwareDevicePrimaryType device_type{EHardwareDevicePrimaryType::Unspecified};
    FKey current_key;
    FKey default_key;
    TOptional<FControlChordBindingView> chord;
};
}
