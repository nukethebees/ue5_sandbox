#pragma once

#include <CoreMinimal.h>

#if WITH_EDITOR
class UWorld;

namespace ioj {
SPACEGAME_API auto create_level_settings_file(UWorld const* world) -> TOptional<FString>;
}
#endif
