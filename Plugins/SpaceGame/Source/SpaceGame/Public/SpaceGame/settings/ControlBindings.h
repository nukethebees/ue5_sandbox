#pragma once

#include <ioj/settings/control_bindings.h>
#include <SpaceGame/settings/ControlBindingView.h>

#include <SandboxCoreEngine/strings.h>

namespace ioj {
inline auto can_hold_chord_key(FKey const& key) -> bool {
    return key.IsValid() && !key.IsAxis1D() && !key.IsAxis2D() && !key.IsAxis3D() &&
           key != EKeys::MouseScrollUp && key != EKeys::MouseScrollDown;
}

inline auto native_binding_key(FKey const& key) -> PhysicalKey {
    return try_parse_serialized_physical_key(to_string(key.GetFName().ToString()))
        .value_or(PhysicalKey::Unbound);
}

inline auto control_binding_identity(FControlBindingAddress const& address,
                                     EHardwareDevicePrimaryType const device_type)
    -> ControlBindingIdentity {
    return control_binding_identity(address, static_cast<std::uint32_t>(device_type));
}

inline auto control_binding_matches_device(FControlBindingView const& binding,
                                           EHardwareDevicePrimaryType const device_type) -> bool {
    return control_binding_matches_device(
        static_cast<std::uint32_t>(binding.device_type),
        static_cast<std::uint32_t>(device_type),
        static_cast<std::uint32_t>(EHardwareDevicePrimaryType::Unspecified));
}

inline auto native_control_binding(FControlBindingView const& binding) -> ControlBindingSnapshot {
    return {.address = binding.address,
            .current_key = native_binding_key(binding.current_key),
            .default_key = native_binding_key(binding.default_key),
            .chord = binding.chord.IsSet() ? std::optional{binding.chord->address} : std::nullopt};
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
