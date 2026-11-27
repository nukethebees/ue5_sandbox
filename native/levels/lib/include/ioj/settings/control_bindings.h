#pragma once

#include <ioj/settings/control_binding_identity.h>
#include <ioj/settings/control_binding_snapshot.h>

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace ioj {
inline auto control_binding_identity(ControlBindingAddress const& address,
                                     std::uint32_t const device_type) -> ControlBindingIdentity {
    return {address.mapping, address.slot, device_type};
}

inline auto control_binding_matches_device(std::uint32_t const binding_device,
                                           std::uint32_t const requested_device,
                                           std::uint32_t const unspecified_device) -> bool {
    return requested_device == unspecified_device || binding_device == requested_device;
}

inline auto control_bindings_changed(std::span<ControlBindingSnapshot const> const current,
                                     std::span<ControlBindingSnapshot const> const applied)
    -> bool {
    if (current.size() != applied.size()) {
        return true;
    }
    for (auto const& binding : current) {
        auto const found{
            std::ranges::find(applied, binding.address, &ControlBindingSnapshot::address)};
        if (found == applied.end() || binding.current_key != found->current_key) {
            return true;
        }
    }
    return false;
}

inline auto control_binding_conflicts(std::span<ControlBindingSnapshot const> const bindings,
                                      ControlBindingAddress const& address,
                                      PhysicalKey const key) -> std::vector<std::size_t> {
    std::vector<std::size_t> result;
    auto const target{std::ranges::find(bindings, address, &ControlBindingSnapshot::address)};
    if (target == bindings.end() || key == PhysicalKey::Unbound) {
        return result;
    }
    auto const count{bindings.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const& binding{bindings[index]};
        if (target->chord == binding.chord &&
            binding.address.profile_index == address.profile_index &&
            is_gamepad_mapping(binding.address.mapping) == is_gamepad_mapping(address.mapping) &&
            binding.address != address && binding.current_key == key) {
            result.push_back(index);
        }
    }
    return result;
}

inline auto control_chord_binding_conflicts(std::span<ControlBindingSnapshot const> const bindings,
                                            ControlBindingAddress const& address,
                                            PhysicalKey const activator_key,
                                            PhysicalKey const action_key)
    -> std::vector<std::size_t> {
    std::vector<std::size_t> result;
    auto const target{std::ranges::find(bindings, address, &ControlBindingSnapshot::address)};
    if (target == bindings.end() || !target->chord || activator_key == PhysicalKey::Unbound ||
        action_key == PhysicalKey::Unbound) {
        return result;
    }
    auto const count{bindings.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const& binding{bindings[index]};
        if (binding.address.profile_index != address.profile_index ||
            is_gamepad_mapping(binding.address.mapping) != is_gamepad_mapping(address.mapping) ||
            binding.address == address || binding.address == *target->chord) {
            continue;
        }
        if ((!binding.chord && binding.current_key == activator_key) ||
            (binding.chord == target->chord && binding.current_key == action_key)) {
            result.push_back(index);
        }
    }
    return result;
}
}
