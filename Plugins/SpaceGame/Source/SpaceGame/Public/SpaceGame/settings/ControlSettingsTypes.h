#pragma once

#include "SpaceGame/input/ControlBindingMetadata.h"
#include <ioj/settings/control_bindings.h>

#include "CoreMinimal.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace ioj {

inline auto can_hold_chord_key(FKey const key) -> bool {
    return key.IsValid() && !key.IsAxis1D() && !key.IsAxis2D() && !key.IsAxis3D() &&
           key != EKeys::MouseScrollUp && key != EKeys::MouseScrollDown;
}

struct FControlBindingAddress {
    FString profile_id;
    FName mapping_name;
    FName hardware_device_id;
    EPlayerMappableKeySlot slot{EPlayerMappableKeySlot::Unspecified};

    auto operator==(FControlBindingAddress const&) const -> bool = default;
};

inline auto native_control_binding_address(FControlBindingAddress const& address)
    -> ControlBindingAddress {
    return {TCHAR_TO_UTF8(*address.profile_id),
            TCHAR_TO_UTF8(*address.mapping_name.ToString().ToLower()),
            TCHAR_TO_UTF8(*address.hardware_device_id.ToString().ToLower()),
            static_cast<std::uint32_t>(address.slot)};
}

inline auto engine_control_binding_address(ControlBindingAddress const& address)
    -> FControlBindingAddress {
    return {UTF8_TO_TCHAR(address.profile_id.c_str()),
            FName{UTF8_TO_TCHAR(address.mapping_name.c_str())},
            FName{UTF8_TO_TCHAR(address.hardware_device_id.c_str())},
            static_cast<EPlayerMappableKeySlot>(address.slot)};
}

inline auto control_binding_identity(FControlBindingAddress const& address,
                                     EHardwareDevicePrimaryType const device_type)
    -> ControlBindingIdentity {
    return control_binding_identity(native_control_binding_address(address),
                                    static_cast<std::uint32_t>(device_type));
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
    EShipControlScope scope{EShipControlScope::General};
    EHardwareDevicePrimaryType device_type{EHardwareDevicePrimaryType::Unspecified};
    FKey current_key;
    FKey default_key;
    TOptional<FControlChordBindingView> chord;
    bool modified{};
};

inline auto control_binding_matches_device(FControlBindingView const& binding,
                                           EHardwareDevicePrimaryType const device_type) -> bool {
    return control_binding_matches_device(
        static_cast<std::uint32_t>(binding.device_type),
        static_cast<std::uint32_t>(device_type),
        static_cast<std::uint32_t>(EHardwareDevicePrimaryType::Unspecified));
}

inline auto native_control_binding(FControlBindingView const& binding) -> ControlBindingSnapshot {
    return {.address = native_control_binding_address(binding.address),
            .current_key = binding.current_key.IsValid()
                             ? TCHAR_TO_UTF8(*binding.current_key.GetFName().ToString().ToLower())
                             : "",
            .default_key = binding.default_key.IsValid()
                             ? TCHAR_TO_UTF8(*binding.default_key.GetFName().ToString().ToLower())
                             : "",
            .scope = static_cast<std::uint32_t>(binding.scope),
            .general_scope = binding.scope == EShipControlScope::General,
            .chord = binding.chord.IsSet()
                       ? std::optional{native_control_binding_address(binding.chord->address)}
                       : std::nullopt};
}

inline auto native_control_bindings(TConstArrayView<FControlBindingView> const bindings)
    -> std::vector<ControlBindingSnapshot> {
    std::vector<ControlBindingSnapshot> result;
    result.reserve(bindings.Num());
    for (auto const& binding : bindings) {
        result.push_back(native_control_binding(binding));
    }
    return result;
}
} // namespace ioj
