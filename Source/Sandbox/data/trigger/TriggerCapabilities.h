#pragma once

#include <array>

#include "CoreMinimal.h"

#include "TriggerCapabilities.generated.h"

UENUM(BlueprintType)
enum class ETriggerCapability : uint8 { None, Humanoid, Mechanical, Magical, Electronic, Physical };

UENUM(BlueprintType)
enum class ETriggerForm : uint8 { Unknown, PlayerInteraction, Activation, Timer, Collision };

struct FTriggerCapabilities {
    static constexpr uint8 MAX_CAPABILITIES{8};
  private:
    std::array<ETriggerCapability, MAX_CAPABILITIES> capabilities_{};
    uint8 count_{0};
  public:
    void add_capability(ETriggerCapability cap) {
        if (count_ < MAX_CAPABILITIES && cap != ETriggerCapability::None) {
            capabilities_[count_++] = cap;
        }
    }

    bool has_capability(ETriggerCapability cap) const {
        for (uint8 i{0}; i < count_; ++i) {
            if (capabilities_[i] == cap) {
                return true;
            }
        }
        return false;
    }

    uint8 get_count() const { return count_; }
};
