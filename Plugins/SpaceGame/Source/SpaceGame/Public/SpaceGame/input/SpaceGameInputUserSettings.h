#pragma once

#include "UserSettings/EnhancedInputUserSettings.h"

#include "SpaceGameInputUserSettings.generated.h"

namespace ioj {

// Kept so that existing Enhanced Input save slots can be deserialized during migration.

UCLASS()
class SPACEGAME_API USpaceGameInputUserSettings final : public UEnhancedInputUserSettings {
    GENERATED_BODY()
};

} // namespace ioj
