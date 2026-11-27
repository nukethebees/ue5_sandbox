#pragma once

#include <ioj/settings/control_bindings.h>
#include <SpaceGame/settings/ControlBindingView.h>

#include <SandboxCoreEngine/strings.h>

namespace ioj {
inline auto can_hold_chord_key(FKey const& key) -> bool {
    return key.IsValid() && !key.IsAxis1D() && !key.IsAxis2D() && !key.IsAxis3D() &&
           key != EKeys::MouseScrollUp && key != EKeys::MouseScrollDown;
}

inline auto native_control_binding_address(FControlBindingAddress const& address)
    -> ControlBindingAddress {
    return {to_string(address.profile_id),
            to_string(address.mapping_name.ToString().ToLower()),
            to_string(address.hardware_device_id.ToString().ToLower()),
            address.slot};
}

inline auto engine_control_binding_address(ControlBindingAddress const& address)
    -> FControlBindingAddress {
    return {to_fstring(address.profile_id),
            FName{*to_fstring(address.mapping_name)},
            FName{*to_fstring(address.hardware_device_id)},
            address.slot};
}

inline auto control_binding_identity(FControlBindingAddress const& address,
                                     EHardwareDevicePrimaryType const device_type)
    -> ControlBindingIdentity {
    return control_binding_identity(native_control_binding_address(address),
                                    static_cast<std::uint32_t>(device_type));
}

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
                             ? to_string(binding.current_key.GetFName().ToString().ToLower())
                             : "",
            .default_key = binding.default_key.IsValid()
                             ? to_string(binding.default_key.GetFName().ToString().ToLower())
                             : "",
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
}
