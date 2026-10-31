#pragma once

#include <CoreMinimal.h>
#include <UserSettings/EnhancedInputUserSettings.h>

namespace ioj {
struct FControlBindingAddress {
    auto operator==(FControlBindingAddress const&) const -> bool = default;

    FString profile_id;
    FName mapping_name;
    FName hardware_device_id;
    EPlayerMappableKeySlot slot{EPlayerMappableKeySlot::Unspecified};
};
}
