#pragma once

#include "CoreMinimal.h"
#include "SpaceGame/input/ControlBindingMetadata.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace ml::ioj {

enum class EControlResetScope : uint8 { AllControls, SettingsOnly };

inline auto control_reset_scope(bool const custom_profile) -> EControlResetScope {
    return custom_profile ? EControlResetScope::SettingsOnly : EControlResetScope::AllControls;
}

inline auto can_hold_chord_key(FKey const key) -> bool {
    return key.IsValid() && !key.IsAxis1D() && !key.IsAxis2D() && !key.IsAxis3D() &&
           key != EKeys::MouseScrollUp && key != EKeys::MouseScrollDown;
}

struct FControlProfileView {
    FString id;
    FString mapping_profile_id;
    FText display_name;
    bool active{};
    bool modified{};
    bool custom{};
};

struct FControlBindingAddress {
    FString profile_id;
    FName mapping_name;
    FName hardware_device_id;
    EPlayerMappableKeySlot slot{EPlayerMappableKeySlot::Unspecified};

    auto operator==(FControlBindingAddress const&) const -> bool = default;
};

struct FControlBindingIdentity {
    FName mapping_name;
    FName hardware_device_id;
    EPlayerMappableKeySlot slot{EPlayerMappableKeySlot::Unspecified};
    EHardwareDevicePrimaryType device_type{EHardwareDevicePrimaryType::Unspecified};

    auto operator==(FControlBindingIdentity const&) const -> bool = default;
};

inline auto control_binding_identity(FControlBindingAddress const& address,
                                     EHardwareDevicePrimaryType const device_type)
    -> FControlBindingIdentity {
    return {
        .mapping_name = address.mapping_name,
        .hardware_device_id = address.hardware_device_id,
        .slot = address.slot,
        .device_type = device_type,
    };
}

struct FControlChordBindingView {
    FControlBindingAddress address;
    FKey current_key;
    FKey default_key;
};

struct FControlBindingView {
    FControlBindingAddress address;
    FText display_name;
    FText display_category;
    EControlBindingGroup display_group{EControlBindingGroup::Flight};
    int32 display_order{};
    EHardwareDevicePrimaryType device_type{EHardwareDevicePrimaryType::Unspecified};
    FKey current_key;
    FKey default_key;
    TOptional<FControlChordBindingView> chord;
    bool modified{};
    bool custom_profile{};
};

inline auto control_binding_matches_device(FControlBindingView const& binding,
                                           EHardwareDevicePrimaryType const device_type) -> bool {
    return device_type == EHardwareDevicePrimaryType::Unspecified ||
           binding.device_type == device_type;
}

} // namespace ml::ioj
