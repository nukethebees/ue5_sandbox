#pragma once

#include "CoreMinimal.h"

namespace ml {
auto get_all_level_names(FName level_directory) -> TArray<FName>;
auto SANDBOX_API format_level_display_name(FName level_name) -> FString;
}
