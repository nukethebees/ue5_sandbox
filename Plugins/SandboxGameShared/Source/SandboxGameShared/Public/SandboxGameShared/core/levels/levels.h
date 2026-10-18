#pragma once

#include "CoreMinimal.h"

namespace ioj {
auto SANDBOXGAMESHARED_API get_all_level_names(FName level_directory) -> TArray<FName>;
auto SANDBOXGAMESHARED_API format_level_display_name(FName level_name) -> FString;
}
