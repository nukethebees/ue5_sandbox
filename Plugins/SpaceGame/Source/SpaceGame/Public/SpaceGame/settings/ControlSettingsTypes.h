#pragma once

#include "CoreMinimal.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace ml::ioj {

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

struct FControlBindingView {
    FControlBindingAddress address;
    FText display_name;
    FText display_category;
    EHardwareDevicePrimaryType device_type{EHardwareDevicePrimaryType::Unspecified};
    FKey current_key;
    FKey default_key;
    bool modified{};
    bool custom_profile{};
};

} // namespace ml::ioj
